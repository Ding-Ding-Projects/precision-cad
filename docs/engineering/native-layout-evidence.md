# Native Qt Quick layout evidence

This application has no browser DOM or CSS layout engine. Its layout equivalent is measured with `QQuickItem::mapRectToScene`, `childrenRect`, implicit sizes, effective visibility, opacity and clipping. Browser `getBoundingClientRect` and CDP ownership assertions do not apply and are not fabricated.

Set `PRECISION_LAYOUT_AUDIT=1` only for a run with an explicit isolated `--profile-directory`. The built application writes a bounded, atomic `layout-audit.json` there. The diagnostic records allowlisted component names and geometry only; it never serializes text, documents, private vocabulary, settings or selected file paths. Production runs without that explicit diagnostic flag write no layout file.

Bind the native version-1 measurement to the exact process/window discovered on the owned hidden desktop, source commit, runtime receipt and genuine window capture. This is a native Qt receipt, not a claimed pass of the browser-specific CSS/CDP validator. Both raw captures and measured native geometry must support a layout conclusion.

## Workspace geometry regression

The documented and enforced minimum client area is 800 by 600. The QML geometry regression exercises that minimum, the 1024 by 700 working size, and the 1280 by 820 default. For English, Cantonese, and bilingual language modes in both light and dark themes, it proves the `ToolBar` height equals the visible `toolbarFlow` implicit height, all 13 toolbar flow controls remain within that measured boundary, the workspace starts immediately below it with a usable height, and the model help remains reachable. Inspector children including the version provenance and unfinished-feature notice do not exceed the inspector column width. The high-scale bilingual tone-five tuple scrolls the native `ScrollView` to its final notice and proves that item reaches the visible scrollport. Deliberate old toolbar and non-scrollable-inspector QML fixtures are expected to fail their matching contracts before the real surface passes them. These are native `QQuickItem` measurements from the QML scene, not browser geometry claims.
