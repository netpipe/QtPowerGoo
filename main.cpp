#include <QApplication>
#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFileDialog>
#include <QGroupBox>
#include <cmath>
#include <qmath.h>
#include <QPointF>
enum BrushType {
    Brush_Smear,
    Brush_Grow,
    Brush_Shrink,
    Brush_Pinch,
    Brush_Ungoo
};

class GooWidget : public QWidget {
    QImage originalImage, currentImage;
    QPoint lastPos;
    float radius = 100.0f;
    float force = 10.0f;
    BrushType brush = Brush_Smear;

public:
    GooWidget(QWidget *parent = nullptr) : QWidget(parent) {
        QString path = QFileDialog::getOpenFileName(this, "Load Image");
        if (path.isEmpty()) exit(1);
        originalImage.load(path);
        originalImage = originalImage.convertToFormat(QImage::Format_ARGB32);
        currentImage = originalImage.copy();
        setFixedSize(originalImage.size());
    }

    void setBrush(BrushType b) { brush = b; }
    void setRadius(int r) { radius = r; }
    void setForce(int f) { force = f; }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.drawImage(0, 0, currentImage);
    }

    void mousePressEvent(QMouseEvent *e) override {
        lastPos = e->pos();
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        QPointF center = lastPos;
        QPointF dir = e->pos() - lastPos;
        if (!dir.isNull()) {
            applyWarp(center, dir);
            lastPos = e->pos();
            update();
        }
    }

    QColor sampleBilinear(const QImage &img, float x, float y) {
        int x0 = qFloor(x), y0 = qFloor(y);
        int x1 = x0 + 1, y1 = y0 + 1;
        float dx = x - x0, dy = y - y0;

        QColor c00 = img.valid(x0, y0) ? img.pixelColor(x0, y0) : Qt::transparent;
        QColor c10 = img.valid(x1, y0) ? img.pixelColor(x1, y0) : c00;
        QColor c01 = img.valid(x0, y1) ? img.pixelColor(x0, y1) : c00;
        QColor c11 = img.valid(x1, y1) ? img.pixelColor(x1, y1) : c00;

        QColor c0 = QColor::fromRgbF(
            c00.redF() * (1 - dx) + c10.redF() * dx,
            c00.greenF() * (1 - dx) + c10.greenF() * dx,
            c00.blueF() * (1 - dx) + c10.blueF() * dx,
            c00.alphaF() * (1 - dx) + c10.alphaF() * dx
        );

        QColor c1 = QColor::fromRgbF(
            c01.redF() * (1 - dx) + c11.redF() * dx,
            c01.greenF() * (1 - dx) + c11.greenF() * dx,
            c01.blueF() * (1 - dx) + c11.blueF() * dx,
            c01.alphaF() * (1 - dx) + c11.alphaF() * dx
        );

        return QColor::fromRgbF(
            c0.redF() * (1 - dy) + c1.redF() * dy,
            c0.greenF() * (1 - dy) + c1.greenF() * dy,
            c0.blueF() * (1 - dy) + c1.blueF() * dy,
            c0.alphaF() * (1 - dy) + c1.alphaF() * dy
        );
    }

    void applyWarp(QPointF location, QPointF direction) {
        QImage newImage = currentImage;

        for (int y = 0; y < newImage.height(); ++y) {
            for (int x = 0; x < newImage.width(); ++x) {
                QPointF coord(x, y);
                float dist = QLineF(coord, location).length();

                if (dist < radius) {
                    float normDist = 1.0f - (dist / radius);
                    float smoothed = normDist * normDist * (3 - 2 * normDist); // smoothstep
                    QPointF offset;

                    switch (brush) {
                        case Brush_Smear:
                            offset = direction * (force / radius) * smoothed;
                            break;
                        case Brush_Ungoo:
                            offset = -direction * (force / radius) * smoothed;
                            break;
                        case Brush_Grow:
                            offset = QVector2D(coord - location).normalized().toPointF()
 * (force / radius) * smoothed;
                            break;
                        case Brush_Shrink:
                            offset = -QVector2D(coord - location).normalized().toPointF()
 * (force / radius) * smoothed;
                            break;
                        case Brush_Pinch:
                            offset = -(coord - location) * 0.01f * force * smoothed;
                            break;
                    }

                    QPointF srcCoord = coord - offset;
                    QColor color = sampleBilinear(currentImage, srcCoord.x(), srcCoord.y());
                    newImage.setPixelColor(x, y, color);
                }
            }
        }

        currentImage = newImage;
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget *window = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(window);

    // Canvas
    GooWidget *canvas = new GooWidget();

    // Controls
    QHBoxLayout *controls = new QHBoxLayout;

    QSlider *radiusSlider = new QSlider(Qt::Horizontal);
    radiusSlider->setRange(10, 200);
    radiusSlider->setValue(100);
    QObject::connect(radiusSlider, &QSlider::valueChanged, canvas, &GooWidget::setRadius);

    QSlider *forceSlider = new QSlider(Qt::Horizontal);
    forceSlider->setRange(1, 50);
    forceSlider->setValue(10);
    QObject::connect(forceSlider, &QSlider::valueChanged, canvas, &GooWidget::setForce);

    QGroupBox *brushBox = new QGroupBox("Brush");
    QVBoxLayout *brushLayout = new QVBoxLayout;
    QButtonGroup *brushGroup = new QButtonGroup(window);
    QStringList brushNames = {"Smear", "Grow", "Shrink", "Pinch", "Ungoo"};
    for (int i = 0; i < brushNames.size(); ++i) {
        QRadioButton *btn = new QRadioButton(brushNames[i]);
        if (i == 0) btn->setChecked(true);
        brushGroup->addButton(btn, i);
        brushLayout->addWidget(btn);
    }
    QObject::connect(brushGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), [=](int id) {
        canvas->setBrush(static_cast<BrushType>(id));
    });
    brushBox->setLayout(brushLayout);

    // Add controls
    controls->addWidget(new QLabel("Radius"));
    controls->addWidget(radiusSlider);
    controls->addWidget(new QLabel("Force"));
    controls->addWidget(forceSlider);
    controls->addWidget(brushBox);

    // Layout
    mainLayout->addLayout(controls);
    mainLayout->addWidget(canvas);
    window->setLayout(mainLayout);
    window->setWindowTitle("Kai's Power Goo Clone");
    window->show();

    return app.exec();
}
