#pragma once

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringView>
#include <QVector>

#include <optional>

namespace precision::core {

struct Dimension final {
    int lengthExponent = 0;
    int angleExponent = 0;

    constexpr bool operator==(const Dimension &) const = default;
    static constexpr Dimension scalar() { return {}; }
    static constexpr Dimension length() { return {1, 0}; }
    static constexpr Dimension angle() { return {0, 1}; }
};

class Quantity final {
public:
    Quantity(double value, Dimension dimension);
    double value() const noexcept { return m_value; }
    const Dimension &dimension() const noexcept { return m_dimension; }

private:
    double m_value;
    Dimension m_dimension;
};

struct ExpressionDiagnostic final {
    QString code;
    QString message;
    int offset = 0;
    int length = 0;
};

struct EvaluationResult final {
    std::optional<Quantity> quantity;
    QVector<ExpressionDiagnostic> diagnostics;
    bool ok() const noexcept { return quantity.has_value() && diagnostics.isEmpty(); }
};

struct ExpressionLimits final {
    int maxSourceCharacters = 4096;
    int maxTokens = 512;
    int maxDepth = 64;
    int maxNodes = 512;
    int maxOperations = 2048;
    int maxBindings = 256;
    int maxIdentifierCharacters = 128;
};

using NamedExpressions = QMap<QString, QString>;
using NamedQuantities = QMap<QString, Quantity>;

class DimensionalExpression final {
public:
    // Unit-bearing literals use canonical millimetres and radians internally. Unitless literals are scalar.
    static EvaluationResult evaluate(QStringView expression,
                                     const NamedQuantities &bindings = {},
                                     const ExpressionLimits &limits = {});

    // Evaluates named expressions as one cached DAG. On any failure `previous` is returned unchanged.
    static EvaluationResult evaluateBindings(const NamedExpressions &expressions,
                                             NamedQuantities *out,
                                             const NamedQuantities &previous = {},
                                             const ExpressionLimits &limits = {});
};

} // namespace precision::core
