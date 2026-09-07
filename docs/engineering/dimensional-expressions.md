# Dimensional expressions

`DimensionalExpression` is the native, offline foundation for typed parameter expressions. It does not mutate the document model, execute scripts, or use a JavaScript engine.

Quantities are immutable `(value, dimension)` pairs. Length values are stored in millimetres and angles in radians. The initial literal units are `mm`, `cm`, `m`, `in`, `ft`, `rad`, and `deg`; literals without units are scalars. Dimension exponents make area and volume representable without special types.

Supported syntax is arithmetic (`+`, `-`, `*`, `/`, unary signs, parentheses), bounded integer powers, named parameters, and `min`, `max`, `abs`, `sqrt`, `sin`, `cos`, and `tan`. Addition, subtraction, `min`, and `max` require matching dimensions. Trigonometric functions require angles and return scalars. `sqrt` requires a nonnegative value with even dimension exponents.

Diagnostics have stable codes and source spans. The evaluator refuses unknown names or units, dimension mismatches, zero denominators, non-finite values, invalid function arity or domain, invalid powers, cycles, and all configured source, token, depth, node, operation, binding, and identifier limits.

`evaluateBindings` resolves a deterministic named-expression DAG with cached completed bindings. It does not mutate the caller output unless every binding succeeds, which lets a later document transaction preserve its previous valid parameter snapshot. This layer deliberately has no document schema, controller, or evaluator integration yet.
