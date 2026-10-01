#pragma once

#include "precision_document.h"

#include <QHash>

namespace precision::core {

enum class EvaluationState {
    Ready,
    Suppressed,
};

struct EvaluatedFeature {
    Feature feature;
    EvaluationState state = EvaluationState::Ready;
    QString suppressionSource;
};

struct EvaluationResult {
    Result result;
    QVector<EvaluatedFeature> ordered;

    bool isSuppressed(const QString &id) const;
};

// Produces the stable execution order for a complete immutable document revision.
// Evaluation never mutates the document and a failed result exposes no partial schedule.
class ModelEvaluator final {
public:
    static EvaluationResult evaluate(const DocumentRecord &record);
};

} // namespace precision::core
