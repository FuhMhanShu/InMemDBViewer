#include "memdbviewer/output_panel.hpp"
#include "memdbviewer/viewer_core.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QToolButton>
#include "tool_button.hpp"
#include <QTextBrowser>
#include <QTime>
#include <QVBoxLayout>

namespace memdb::viewer {

struct OutputPanel::Impl {
    ViewerCore* core = nullptr;
    QTextBrowser* browser = nullptr;
    QCheckBox* onlyErrors = nullptr;
    int total = 0;
    int errors = 0;
    QLabel* countLabel = nullptr;
};

OutputPanel::OutputPanel(QWidget* parent) : QWidget(parent), impl_(std::make_unique<Impl>()) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    auto* bar = new QHBoxLayout();
    auto* title = new QLabel(QStringLiteral("Ausgabe"), this);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    bar->addWidget(title);
    impl_->onlyErrors = new QCheckBox(QStringLiteral("nur Fehler"), this);
    impl_->countLabel = new QLabel(this);
    auto* clearBtn = detail::makeToolButton(this, QStyle::SP_DialogResetButton, QStringLiteral("Leeren"));
    bar->addStretch();
    bar->addWidget(impl_->onlyErrors);
    bar->addWidget(impl_->countLabel);
    bar->addWidget(clearBtn);
    layout->addLayout(bar);
    impl_->browser = new QTextBrowser(this);
    impl_->browser->setOpenExternalLinks(false);
    layout->addWidget(impl_->browser, 1);
    connect(clearBtn, &QToolButton::clicked, this, &OutputPanel::clear);
    connect(impl_->onlyErrors, &QCheckBox::toggled, this, [this] {
        impl_->browser->setProperty("onlyErrors", impl_->onlyErrors->isChecked());
    });
    updateCount();
}

OutputPanel::~OutputPanel() = default;

void OutputPanel::setCore(ViewerCore* core) {
    if (impl_->core)
        disconnect(impl_->core, nullptr, this, nullptr);
    impl_->core = core;
    if (core)
        connect(core, &ViewerCore::logged, this, &OutputPanel::log);
}

ViewerCore* OutputPanel::core() const { return impl_->core; }

void OutputPanel::updateCount() {
    impl_->countLabel->setText(QStringLiteral("%1 Einträge (%2 Fehler)").arg(impl_->total).arg(impl_->errors));
}

void OutputPanel::log(const QString& message, bool error) {
    if (impl_->onlyErrors->isChecked() && !error)
        return;
    ++impl_->total;
    if (error)
        ++impl_->errors;
    const QString time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    const QString color = error ? QStringLiteral("#c62828") : QStringLiteral("#1b5e20");
    const QString icon = error ? QStringLiteral("✗") : QStringLiteral("✓");
    impl_->browser->append(QStringLiteral("<span style='color:#888'>[%1]</span> <span style='color:%2'>%3 %4</span>")
                               .arg(time, color, icon, message.toHtmlEscaped()));
    updateCount();
}

void OutputPanel::clear() {
    impl_->browser->clear();
    impl_->total = 0;
    impl_->errors = 0;
    updateCount();
}

} // namespace memdb::viewer
