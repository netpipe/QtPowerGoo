#include <QApplication>
#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMouseEvent>
#include <QFileDialog>
#include <QSlider>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QTransform>
#include <QObject>

enum ToolMode {
    Tool_PaintA,
    Tool_PaintB,
    Tool_Smear,
    Tool_Smooth
};

class FusionCanvas : public QWidget {
    QImage imgA, imgB, mask, fusion;
    float radius = 50.0f;
    ToolMode mode = Tool_PaintA;

public:
    FusionCanvas(const QString &pathA, const QString &pathB, QWidget *parent = nullptr) : QWidget(parent) {
        imgA.load(pathA);
        imgB.load(pathB);
        imgA = imgA.convertToFormat(QImage::Format_ARGB32);
        imgB = imgB.convertToFormat(QImage::Format_ARGB32);
        imgA = imgA.scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        imgB = imgB.scaled(imgA.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        setFixedSize(imgA.size());
        mask = QImage(imgA.size(), QImage::Format_Grayscale8);
        mask.fill(128); // 128 = even mix of A and B

        updateFusion();
    }

    void setRadius(int r) { radius = r; }
    void setTool(ToolMode m) { mode = m; }

    void updateFusion() {
        fusion = QImage(imgA.size(), QImage::Format_ARGB32);
        for (int y = 0; y < fusion.height(); ++y) {
            for (int x = 0; x < fusion.width(); ++x) {
                int alpha = mask.pixelColor(x, y).red(); // 0–255
                float blend = alpha / 255.0f;
                QColor cA = imgA.pixelColor(x, y);
                QColor cB = imgB.pixelColor(x, y);
                QColor mix = QColor::fromRgb(
                    cA.red()   * (1 - blend) + cB.red()   * blend,
                    cA.green() * (1 - blend) + cB.green() * blend,
                    cA.blue()  * (1 - blend) + cB.blue()  * blend
                );
                fusion.setPixelColor(x, y, mix);
            }
        }
        update();
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.drawImage(0, 0, fusion);
    }
QPoint lastPos;
    void mouseMoveEvent(QMouseEvent *e) override {
        if (mode == Tool_Smear && (e->buttons() & Qt::LeftButton)) {
            QPointF delta = e->pos() - lastPos;
            if (delta.manhattanLength() < 1)
                return;

            QPainter p(&mask);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.setRenderHint(QPainter::Antialiasing);

            int patchSize = radius * 2;
            QRect area(lastPos - QPoint(radius, radius), QSize(patchSize, patchSize));
            QImage patch = mask.copy(area);

            QPointF offset = delta; // smear direction
            QRectF targetRect = QRectF(area).translated(offset.x(), offset.y());


            // Use semi-transparent blending to create a dragging effect
            p.setOpacity(0.9); // blend rather than hard overwrite
            p.drawImage(targetRect, patch);

            updateFusion();
            lastPos = e->pos();
        }
applyBrush(e->pos());

    }

    void mousePressEvent(QMouseEvent *e) override {
        applyBrush(e->pos());
    }

    void applyBrush(QPoint pos) {
        if (mode == Tool_Smooth) {
            int size = radius * 2;
            QRect area(pos - QPoint(radius, radius), QSize(size, size));

            QImage patch = mask.copy(area);
            QImage blurred = patch.scaled(radius, radius, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                  .scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            QPainter p(&mask);
            p.setOpacity(0.7); // soft blend
            p.drawImage(area.topLeft(), blurred);
            updateFusion();
            return;
        }


        QPainter p(&mask);

        QRadialGradient g(pos, radius);
        QColor existing = mask.pixelColor(pos);

        if (mode == Tool_PaintA) {
            g.setColorAt(0.0, QColor(0, 0, 0));
        } else if (mode == Tool_PaintB) {
            g.setColorAt(0.0, QColor(255, 255, 255));
        } else {
            return;
        }
        g.setColorAt(1.0, existing);  // feather from existing value

        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setBrush(g);
        p.setPen(Qt::NoPen);
      //  p.drawEllipse(pos, radius, radius);
        p.drawEllipse(QPointF(pos), radius, radius);

        updateFusion();
    }

    void flipImageA(bool horiz) {
        QTransform t;
        t.scale(horiz ? -1 : 1, horiz ? 1 : -1);
        imgA = imgA.transformed(t, Qt::SmoothTransformation);
        updateFusion();
    }

    void flipImageB(bool horiz) {
        QTransform t;
        t.scale(horiz ? -1 : 1, horiz ? 1 : -1);
        imgB = imgB.transformed(t, Qt::SmoothTransformation);
        updateFusion();
    }

    QImage getImageA() const { return imgA; }
    QImage getImageB() const { return imgB; }
    QImage getFusion() const { return fusion; }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QString pathA = QFileDialog::getOpenFileName(nullptr, "Select Face A");
    QString pathB = QFileDialog::getOpenFileName(nullptr, "Select Face B");
    if (pathA.isEmpty() || pathB.isEmpty()) return 0;

    QWidget *window = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(window);
    QHBoxLayout *topLayout = new QHBoxLayout;
    QHBoxLayout *controls = new QHBoxLayout;

    // Load fusion canvas
    FusionCanvas *canvas = new FusionCanvas(pathA, pathB);

    // Views for A, Fusion, B
    QLabel *viewA = new QLabel;
    QLabel *viewB = new QLabel;
    QLabel *viewF = new QLabel;

    viewA->setPixmap(QPixmap::fromImage(canvas->getImageA()));
    viewB->setPixmap(QPixmap::fromImage(canvas->getImageB()));
    viewF->setPixmap(QPixmap::fromImage(canvas->getFusion()));

    viewF->setPixmap(QPixmap::fromImage(canvas->getFusion()));

    topLayout->addWidget(viewA);
    topLayout->addWidget(canvas);
    topLayout->addWidget(viewB);

    // Brush radius slider
    QSlider *radiusSlider = new QSlider(Qt::Horizontal);
    radiusSlider->setRange(10, 100);
    radiusSlider->setValue(50);
    QObject::connect(radiusSlider, &QSlider::valueChanged, canvas, &FusionCanvas::setRadius);

    // Tool selection
    QButtonGroup *tools = new QButtonGroup;
    QRadioButton *paintA = new QRadioButton("Paint A");
    QRadioButton *paintB = new QRadioButton("Paint B");
    QRadioButton *smear = new QRadioButton("Smear");
    QRadioButton *smooth = new QRadioButton("Smooth");
    paintA->setChecked(true);
    tools->addButton(paintA, Tool_PaintA);
    tools->addButton(paintB, Tool_PaintB);
    tools->addButton(smear, Tool_Smear);
    tools->addButton(smooth, Tool_Smooth);

    QObject::connect(tools, QOverload<int>::of(&QButtonGroup::buttonClicked), [=](int id) {
        canvas->setTool(static_cast<ToolMode>(id));
    });

    // Flip buttons
    QPushButton *flipA = new QPushButton("Flip A");
    QPushButton *flipB = new QPushButton("Flip B");

    QObject::connect(flipA, &QPushButton::clicked, [=]() {
        canvas->flipImageA(true);
        viewA->setPixmap(QPixmap::fromImage(canvas->getImageA()));
    });
    QObject::connect(flipB, &QPushButton::clicked, [=]() {
        canvas->flipImageB(true);
        viewB->setPixmap(QPixmap::fromImage(canvas->getImageB()));
    });

    controls->addWidget(paintA);
    controls->addWidget(paintB);
    controls->addWidget(smear);
    controls->addWidget(smooth);
    controls->addWidget(new QLabel("Brush Radius"));
    controls->addWidget(radiusSlider);
    controls->addWidget(flipA);
    controls->addWidget(flipB);

    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(controls);
    window->setLayout(mainLayout);
    window->setWindowTitle("Kai's Fusion Room Clone");
    window->show();

    return app.exec();
}
