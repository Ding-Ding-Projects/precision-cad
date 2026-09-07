#include "dimensional_expression.h"

#include <cmath>
#include <algorithm>
#include <functional>
#include <limits>
#include <numbers>

namespace precision::core {
namespace {
constexpr int kMaxExponent = 12;

ExpressionDiagnostic error(QString code, QString message, int offset, int length = 1) {
    return {std::move(code), std::move(message), offset, length};
}

bool finite(double value) { return std::isfinite(value); }
bool validDimension(Dimension dimension) {
    return std::abs(dimension.lengthExponent) <= kMaxExponent && std::abs(dimension.angleExponent) <= kMaxExponent;
}
bool validIdentifier(QStringView identifier, const ExpressionLimits &limits) {
    if (identifier.isEmpty() || identifier.size() > limits.maxIdentifierCharacters)
        return false;
    if (!(identifier.front().isLetter() || identifier.front() == u'_'))
        return false;
    for (QChar c : identifier)
        if (!(c.isLetterOrNumber() || c == u'_')) return false;
    return true;
}

class Parser final {
public:
    Parser(QStringView source, const NamedQuantities &bindings, const ExpressionLimits &limits)
        : m_source(source), m_bindings(bindings), m_limits(limits) {}

    EvaluationResult parse() {
        if (m_source.size() > m_limits.maxSourceCharacters)
            return fail("source_too_long", "Expression exceeds the source character limit.", 0, m_source.size());
        skip();
        auto value = expression(0);
        skip();
        if (!m_failed && m_position != m_source.size()) failNow("unexpected_token", "Unexpected token.", m_position);
        if (m_failed) return {std::nullopt, {m_diagnostic}};
        return {value, {}};
    }

private:
    std::optional<Quantity> expression(int depth) {
        if (depth > m_limits.maxDepth) return failValue("depth_limit", "Expression nesting limit exceeded.", m_position);
        auto left = term(depth + 1);
        while (left && !m_failed) {
            skip(); const QChar op = current();
            if (op != u'+' && op != u'-') break;
            const int opAt = m_position++; if (!consumeOperation()) return {}; auto right = term(depth + 1); if (!right) return {};
            if (left->dimension() != right->dimension()) return failValue("incompatible_dimensions", "Addition and subtraction require matching dimensions.", opAt);
            left = Quantity(op == u'+' ? left->value() + right->value() : left->value() - right->value(), left->dimension());
            if (!checkFinite(*left, opAt)) return {};
        }
        return left;
    }
    std::optional<Quantity> term(int depth) {
        auto left = unary(depth + 1);
        while (left && !m_failed) {
            skip(); const QChar op = current();
            if (op != u'*' && op != u'/') break;
            const int opAt = m_position++; if (!consumeOperation()) return {}; auto right = unary(depth + 1); if (!right) return {};
            if (op == u'/' && right->value() == 0.0) return failValue("division_by_zero", "Division by zero is not allowed.", opAt);
            Dimension d{left->dimension().lengthExponent + (op == u'*' ? right->dimension().lengthExponent : -right->dimension().lengthExponent),
                        left->dimension().angleExponent + (op == u'*' ? right->dimension().angleExponent : -right->dimension().angleExponent)};
            if (!validDimension(d)) return failValue("dimension_overflow", "Dimension exponent exceeds the supported range.", opAt);
            left = Quantity(op == u'*' ? left->value() * right->value() : left->value() / right->value(), d);
            if (!checkFinite(*left, opAt)) return {};
        }
        return left;
    }
    std::optional<Quantity> unary(int depth) {
        skip();
        if (current() == u'+' || current() == u'-') { const bool negate = current() == u'-'; ++m_position; if (!consumeOperation()) return {}; auto v = unary(depth + 1); return v && negate ? std::optional<Quantity>(Quantity(-v->value(), v->dimension())) : v; }
        auto left = primary(depth + 1);
        skip();
        if (left && current() == u'^') {
            const int at = m_position++; if (!consumeOperation()) return {}; auto exponent = primary(depth + 1);
            if (!exponent) return {};
            if (exponent->dimension() != Dimension::scalar() || std::floor(exponent->value()) != exponent->value() || std::abs(exponent->value()) > kMaxExponent)
                return failValue("invalid_power", "Power must be a bounded scalar integer.", at);
            const int n = static_cast<int>(exponent->value());
            Dimension d{left->dimension().lengthExponent * n, left->dimension().angleExponent * n};
            if (!validDimension(d)) return failValue("dimension_overflow", "Dimension exponent exceeds the supported range.", at);
            left = Quantity(std::pow(left->value(), n), d); if (!checkFinite(*left, at)) return {};
        }
        return left;
    }
    std::optional<Quantity> primary(int depth) {
        if (++m_nodes > m_limits.maxNodes) return failValue("node_limit", "Expression node limit exceeded.", m_position);
        if (++m_tokens > m_limits.maxTokens) return failValue("token_limit", "Expression token limit exceeded.", m_position);
        skip(); const int start = m_position;
        if (current() == u'(') { ++m_position; auto value = expression(depth + 1); skip(); if (current() != u')') return failValue("missing_parenthesis", "Expected closing parenthesis.", m_position); ++m_position; return value; }
        if (current().isDigit() || current() == u'.') return number();
        if (current().isLetter() || current() == u'_') {
            const QStringView name = identifier(); if (!validIdentifier(name, m_limits)) return failValue("invalid_identifier", "Invalid identifier.", start, m_position - start);
            skip(); if (current() == u'(') return function(name, start, depth + 1);
            const auto it = m_bindings.constFind(name.toString());
            if (it == m_bindings.cend()) return failValue("unknown_reference", "Unknown named parameter.", start, m_position - start);
            if (!finite(it->value()) || !validDimension(it->dimension())) return failValue("invalid_binding", "Named parameter is not a finite supported quantity.", start, m_position - start);
            return *it;
        }
        return failValue("expected_value", "Expected a number, parameter, function, or parenthesized expression.", start);
    }
    std::optional<Quantity> number() {
        const int start = m_position; bool dot = false;
        while (current().isDigit() || (!dot && current() == u'.')) { dot |= current() == u'.'; ++m_position; }
        bool ok = false; const double raw = m_source.mid(start, m_position - start).toString().toDouble(&ok);
        if (!ok || !finite(raw)) return failValue("invalid_number", "Invalid finite number.", start, m_position - start);
        const int unitStart = m_position; while (current().isLetter()) ++m_position;
        const QString unit = m_source.mid(unitStart, m_position - unitStart).toString();
        if (unit.isEmpty()) return Quantity(raw, Dimension::scalar());
        Quantity converted(raw, Dimension::scalar());
        if (unit == "mm") converted = Quantity(raw, Dimension::length());
        else if (unit == "cm") converted = Quantity(raw * 10.0, Dimension::length());
        else if (unit == "m") converted = Quantity(raw * 1000.0, Dimension::length());
        else if (unit == "in") converted = Quantity(raw * 25.4, Dimension::length());
        else if (unit == "ft") converted = Quantity(raw * 304.8, Dimension::length());
        else if (unit == "rad") converted = Quantity(raw, Dimension::angle());
        else if (unit == "deg") converted = Quantity(raw * std::numbers::pi / 180.0, Dimension::angle());
        else return failValue("unknown_unit", "Unknown unit.", unitStart, m_position - unitStart);
        if (!checkFinite(converted, unitStart)) return {};
        return converted;
    }
    std::optional<Quantity> function(QStringView name, int start, int depth) {
        if (!consumeOperation()) return {};
        ++m_position; QVector<Quantity> args; skip();
        if (current() != u')') while (true) { auto value = expression(depth + 1); if (!value) return {}; args.append(*value); skip(); if (current() != u',') break; ++m_position; }
        if (current() != u')') return failValue("missing_parenthesis", "Expected closing parenthesis.", m_position); ++m_position;
        auto one = [&](const char *fn) -> std::optional<Quantity> { if (args.size() != 1) return failValue("invalid_arity", QString::fromLatin1(fn) + " requires one argument.", start); return args[0]; };
        if (name == u"abs") { auto a = one("abs"); return a ? std::optional<Quantity>(Quantity(std::abs(a->value()), a->dimension())) : std::nullopt; }
        if (name == u"sqrt") { auto a = one("sqrt"); if (!a) return {}; if (a->value() < 0 || a->dimension().lengthExponent % 2 || a->dimension().angleExponent % 2) return failValue("invalid_dimension_function", "sqrt requires nonnegative value and even dimension exponents.", start); return Quantity(std::sqrt(a->value()), {a->dimension().lengthExponent / 2, a->dimension().angleExponent / 2}); }
        if (name == u"sin" || name == u"cos" || name == u"tan") { auto a = one("trigonometric function"); if (!a) return {}; if (a->dimension() != Dimension::angle()) return failValue("invalid_dimension_function", "Trigonometric functions require an angle.", start); return Quantity(name == u"sin" ? std::sin(a->value()) : name == u"cos" ? std::cos(a->value()) : std::tan(a->value()), Dimension::scalar()); }
        if (name == u"min" || name == u"max") { if (args.size() != 2) return failValue("invalid_arity", "min and max require two arguments.", start); if (args[0].dimension() != args[1].dimension()) return failValue("incompatible_dimensions", "min and max require matching dimensions.", start); return Quantity(name == u"min" ? std::min(args[0].value(), args[1].value()) : std::max(args[0].value(), args[1].value()), args[0].dimension()); }
        return failValue("unknown_function", "Unknown function.", start, m_position - start);
    }
    void skip() { while (current().isSpace()) ++m_position; }
    QChar current() const { return m_position < m_source.size() ? m_source[m_position] : QChar(); }
    QStringView identifier() { const int start = m_position; while (current().isLetterOrNumber() || current() == u'_') ++m_position; return m_source.mid(start, m_position - start); }
    bool checkFinite(const Quantity &quantity, int at) { if (finite(quantity.value())) return true; failNow("non_finite", "Expression result is not finite.", at); return false; }
    bool consumeOperation() { if (++m_operations <= m_limits.maxOperations) return true; failNow("operation_limit", "Expression operation limit exceeded.", m_position); return false; }
    std::optional<Quantity> failValue(QString code, QString message, int offset, int length = 1) { failNow(std::move(code), std::move(message), offset, length); return {}; }
    EvaluationResult fail(QString code, QString message, int offset, int length) { return {std::nullopt, {error(std::move(code), std::move(message), offset, length)}}; }
    void failNow(QString code, QString message, int offset, int length = 1) { if (!m_failed) { m_failed = true; m_diagnostic = error(std::move(code), std::move(message), offset, length); } }
    QStringView m_source; const NamedQuantities &m_bindings; const ExpressionLimits &m_limits; int m_position = 0, m_nodes = 0, m_tokens = 0, m_operations = 0; bool m_failed = false; ExpressionDiagnostic m_diagnostic;
};
}

Quantity::Quantity(double value, Dimension dimension) : m_value(value), m_dimension(dimension) {}

EvaluationResult DimensionalExpression::evaluate(QStringView expression, const NamedQuantities &bindings, const ExpressionLimits &limits) {
    return Parser(expression, bindings, limits).parse();
}

EvaluationResult DimensionalExpression::evaluateBindings(const NamedExpressions &expressions, NamedQuantities *out, const NamedQuantities &previous, const ExpressionLimits &limits) {
    if (!out) return {std::nullopt, {error("invalid_output", "Output bindings pointer is required.", 0)}};
    if (expressions.size() > limits.maxBindings) return {std::nullopt, {error("binding_limit", "Named binding limit exceeded.", 0)}};
    for (auto it = expressions.cbegin(); it != expressions.cend(); ++it)
        if (!validIdentifier(it.key(), limits)) return {std::nullopt, {error("invalid_identifier", "Invalid named parameter identifier.", 0, it.key().size())}};
    NamedQuantities resolved; QHash<QString, int> states;
    std::function<EvaluationResult(const QString &)> resolve = [&](const QString &name) -> EvaluationResult {
        if (states.value(name) == 2) return {*resolved.constFind(name), {}};
        if (states.value(name) == 1) return {std::nullopt, {error("expression_cycle", "Named expressions contain a dependency cycle.", 0, name.size())}};
        const auto expr = expressions.constFind(name); if (expr == expressions.cend()) return {std::nullopt, {error("unknown_reference", "Unknown named parameter.", 0, name.size())}};
        states[name] = 1; NamedQuantities visible = previous; visible.insert(resolved);
        // Resolve only complete identifier tokens. This avoids false dependencies such as `a` in `max`.
        const QStringView text(*expr);
        for (int index = 0; index < text.size();) {
            if (!(text[index].isLetter() || text[index] == u'_')) { ++index; continue; }
            const int start = index++;
            while (index < text.size() && (text[index].isLetterOrNumber() || text[index] == u'_')) ++index;
            const QString token = text.mid(start, index - start).toString();
            int after = index; while (after < text.size() && text[after].isSpace()) ++after;
            if ((start > 0 && (text[start - 1].isDigit() || text[start - 1] == u'.')) || (after < text.size() && text[after] == u'(')) continue;
            if (expressions.contains(token)) { auto nested = resolve(token); if (!nested.ok()) return nested; visible.insert(token, *nested.quantity); }
        }
        auto result = evaluate(*expr, visible, limits); if (!result.ok()) return result;
        resolved.insert(name, *result.quantity); states[name] = 2; return result;
    };
    for (auto it = expressions.cbegin(); it != expressions.cend(); ++it) { auto result = resolve(it.key()); if (!result.ok()) return result; }
    *out = resolved; return {Quantity(0.0, Dimension::scalar()), {}};
}

} // namespace precision::core
