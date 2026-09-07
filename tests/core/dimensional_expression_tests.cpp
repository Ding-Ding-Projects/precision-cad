#include "dimensional_expression.h"

#include <QCoreApplication>
#include <QDebug>

#include <cmath>
#include <limits>

using namespace precision::core;

namespace {
int failures = 0;
void expect(bool condition, const char *message) { if (!condition) { qCritical() << message; ++failures; } }
void expectNear(double actual, double expected, const char *message) { expect(std::abs(actual - expected) < 1e-10, message); }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    auto inches = DimensionalExpression::evaluate(u"2mm + 1in");
    expect(inches.ok(), "mixed length units should evaluate");
    expectNear(inches.quantity->value(), 27.4, "length must be canonical millimetres");
    expect(inches.quantity->dimension() == Dimension::length(), "sum must retain length");

    auto area = DimensionalExpression::evaluate(u"sqrt(81mm * 81mm)");
    expect(area.ok() && area.quantity->dimension() == Dimension::length(), "sqrt area should return length");
    expectNear(area.quantity->value(), 81.0, "sqrt area should return canonical length");
    auto trig = DimensionalExpression::evaluate(u"sin(90deg)");
    expect(trig.ok() && trig.quantity->dimension() == Dimension::scalar(), "trig angle should yield scalar");
    expectNear(trig.quantity->value(), 1.0, "degrees should normalize to radians");

    NamedExpressions expressions{{"width", "2in"}, {"doubleWidth", "width * 2"}};
    NamedQuantities output;
    auto bindings = DimensionalExpression::evaluateBindings(expressions, &output);
    expect(bindings.ok() && output.contains("doubleWidth"), "bindings should propagate through DAG");
    expectNear(output.constFind("doubleWidth")->value(), 101.6, "dependent binding should use changed source");
    const NamedQuantities snapshot = output;
    auto cycle = DimensionalExpression::evaluateBindings({{"a", "b"}, {"b", "a"}}, &output, snapshot);
    expect(!cycle.ok() && output.constFind("doubleWidth")->value() == snapshot.constFind("doubleWidth")->value(), "failed bindings must preserve caller snapshot");
    auto unknown = DimensionalExpression::evaluate(u"missing + 1mm");
    expect(!unknown.ok() && unknown.diagnostics.first().code == "unknown_reference", "unknown reference must diagnose");
    expect(!DimensionalExpression::evaluate(u"sin(1mm)").ok(), "trig length must be rejected");
    expect(!DimensionalExpression::evaluate(u"1 + 1mm").ok(), "plain numbers remain scalar");
    auto mismatch = DimensionalExpression::evaluate(u"1 + 1mm");
    expect(!mismatch.ok() && mismatch.diagnostics.first().offset == 2, "binary diagnostic should highlight operator");
    const auto conversionOverflow = DimensionalExpression::evaluate(QStringLiteral("1") + QString(307, u'0') + QStringLiteral("m"));
    expect(!conversionOverflow.ok() && conversionOverflow.diagnostics.first().code == "non_finite", "overflowing unit conversion must be rejected");
    NamedQuantities invalidBinding{{"bad", Quantity(std::numeric_limits<double>::quiet_NaN(), Dimension::length())}};
    expect(!DimensionalExpression::evaluate(u"bad", invalidBinding).ok(), "non-finite external quantity must be rejected");
    auto unitNamedBinding = DimensionalExpression::evaluateBindings({{"mm", "2mm"}}, &output);
    expect(unitNamedBinding.ok(), "unit suffix must not create a false cycle");
    ExpressionLimits tiny; tiny.maxTokens = 1;
    expect(!DimensionalExpression::evaluate(u"1 + 2", {}, tiny).ok(), "token limit must be enforced");
    tiny = {}; tiny.maxOperations = 1;
    expect(!DimensionalExpression::evaluate(u"1 + 2 + 3", {}, tiny).ok(), "operation limit must be enforced");

    QString deep; for (int i = 0; i < 100; ++i) deep += '('; deep += '1'; for (int i = 0; i < 100; ++i) deep += ')';
    expect(!DimensionalExpression::evaluate(deep).ok(), "deep input must be bounded");
    for (const QString &input : {QString(), QStringLiteral("1e9999"), QStringLiteral("1 / 0"), QStringLiteral("sqrt(-1)")})
        expect(!DimensionalExpression::evaluate(input).ok(), "malformed fuzz sample must be bounded and diagnosed");
    return failures == 0 ? 0 : 1;
}
