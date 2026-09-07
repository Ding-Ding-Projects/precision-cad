# Native Qt Quick layout evidence

This application has no browser DOM or CSS layout engine. Its layout equivalent is measured with `QQuickItem::mapRectToScene`, `childrenRect`, implicit sizes, effective visibility, opacity and clipping. Browser `getBoundingClientRect` and CDP ownership assertions do not apply and are not fabricated.

Set `PRECISION_LAYOUT_AUDIT=1` only for a run with an explicit isolated `--profile-directory`. The built application writes a bounded, atomic `layout-audit.json` there. The diagnostic records allowlisted component names and geometry only; it never serializes text, documents, private vocabulary, settings or selected file paths. Production runs without that explicit diagnostic flag write no layout file.

Bind the native version-1 measurement to the exact process/window discovered on the owned hidden desktop, source commit, runtime receipt and genuine window capture. This is a native Qt receipt, not a claimed pass of the browser-specific CSS/CDP validator. Both raw captures and measured native geometry must support a layout conclusion.
