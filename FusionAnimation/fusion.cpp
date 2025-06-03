// QFusionRoom FX - Single File Demo (Qt 5.12)
// Features: Face Fusion + Animation + Fractal + Fun FX

#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QImage>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QFileDialog>
#include <QtMath>

const QSize canvasSize(512, 512);

QImage loadAndResize(const QString& path) {
    QImage img(path);
    return img.scaled(canvasSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation).copy(QRect(QPoint(0, 0), canvasSize));
}

QImage generateFractal(double zoom, int time) {
    QImage img(canvasSize, QImage::Format_RGB32);
    const int w = canvasSize.width(), h = canvasSize.height();
    double cx = -0.7, cy = 0.27015;
    #pragma omp parallel for
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double zx = 1.5 * (x - w / 2) / (0.5 * zoom * w);
            double zy = (y - h / 2) / (0.5 * zoom * h);
            int i = 0;
            while (zx*zx + zy*zy < 4 && i < 255) {
                double tmp = zx * zx - zy * zy + cx + 0.1 * sin(time * 0.05);
                zy = 2.0 * zx * zy + cy;
                zx = tmp;
                ++i;
            }
            img.setPixel(x, y, qRgb(i, i, i));
        }
    }
    return img;
}

QImage blendFusion(const QImage& imgA, const QImage& imgB, float blendAmount, float warpAmount, int time) {
    QImage result(canvasSize, QImage::Format_RGB32);
    for (int y = 0; y < canvasSize.height(); ++y) {
        for (int x = 0; x < canvasSize.width(); ++x) {
            int xx = x + warpAmount * sin(2 * 3.14 * y / 128 + time * 0.05);
            int yy = y + warpAmount * cos(2 * 3.14 * x / 128 + time * 0.05);
            if (xx < 0 || xx >= canvasSize.width() || yy < 0 || yy >= canvasSize.height()) continue;

            QColor ca = imgA.pixelColor(x, y);
            QColor cb = imgB.pixelColor(xx, yy);

            QColor mixed(
                ca.red() * (1 - blendAmount) + cb.red() * blendAmount,
                ca.green() * (1 - blendAmount) + cb.green() * blendAmount,
                ca.blue() * (1 - blendAmount) + cb.blue() * blendAmount
            );
            result.setPixelColor(x, y, mixed);
        }
    }
    return result;
}

QImage applyTileSpin(const QImage& src, float angleDeg, int tiles) {
    QImage result(canvasSize, QImage::Format_RGB32);
    QPainter p(&result);
    int tileW = canvasSize.width() / tiles;
    int tileH = canvasSize.height() / tiles;

    for (int y = 0; y < tiles; ++y) {
        for (int x = 0; x < tiles; ++x) {
            QRect rect(x * tileW, y * tileH, tileW, tileH);
            p.save();
            p.translate(rect.center());
            p.rotate(angleDeg);
            p.translate(-rect.center());
            p.drawImage(rect, src, rect);
            p.restore();
        }
    }
    return result;
}

QImage applyGoovieEffect(const QImage& src, float strength, int time) {
    QImage result(canvasSize, QImage::Format_RGB32);
    int w = canvasSize.width(), h = canvasSize.height();
    QPoint center(w/2, h/2);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            QPointF dir = QPointF(x - center.x(), y - center.y());
            float dist = std::hypot(dir.x(), dir.y());
            float factor = strength * std::sin(dist / 20.0 - time * 0.1);
            QPoint srcPt = QPoint(x + dir.x() * factor * 0.01, y + dir.y() * factor * 0.01);
            if (srcPt.x() >= 0 && srcPt.x() < w && srcPt.y() >= 0 && srcPt.y() < h)
                result.setPixelColor(x, y, src.pixelColor(srcPt));
        }
    }
    return result;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle("QFusionRoom FX");
    QVBoxLayout* mainLayout = new QVBoxLayout(&window);

    QLabel* preview = new QLabel;
    preview->setFixedSize(canvasSize);
    mainLayout->addWidget(preview);

    QHBoxLayout* sliders = new QHBoxLayout;
    QSlider *blendSlider = new QSlider(Qt::Horizontal);
    QSlider *zoomSlider = new QSlider(Qt::Horizontal);
    QSlider *spinSlider = new QSlider(Qt::Horizontal);
    QSlider *warpSlider = new QSlider(Qt::Horizontal);
    blendSlider->setRange(0, 100); zoomSlider->setRange(1, 100);
    spinSlider->setRange(0, 360); warpSlider->setRange(0, 50);
    sliders->addWidget(new QLabel("Blend")); sliders->addWidget(blendSlider);
    sliders->addWidget(new QLabel("Zoom")); sliders->addWidget(zoomSlider);
    sliders->addWidget(new QLabel("Spin")); sliders->addWidget(spinSlider);
    sliders->addWidget(new QLabel("Warp")); sliders->addWidget(warpSlider);
    mainLayout->addLayout(sliders);

    QHBoxLayout* buttons = new QHBoxLayout;
    QPushButton* startBtn = new QPushButton("Start Animation");
    QPushButton* stopBtn = new QPushButton("Stop");
    buttons->addWidget(startBtn); buttons->addWidget(stopBtn);
    mainLayout->addLayout(buttons);

    QImage imgA = loadAndResize("imgA.png");
    QImage imgB = loadAndResize("imgB.png");
    QTimer* timer = new QTimer;
    int time = 0;

    QObject::connect(timer, &QTimer::timeout, [&]() {
        float blend = blendSlider->value() / 100.0f;
        float zoom = zoomSlider->value();
        float spin = spinSlider->value();
        float warp = warpSlider->value();

        QImage fused = blendFusion(imgA, imgB, blend, warp, time);
        QImage fractal = generateFractal(zoom, time);
        QImage combo(canvasSize, QImage::Format_RGB32);
        QPainter p(&combo);
        p.drawImage(0, 0, fractal);
        p.setOpacity(0.7);
        p.drawImage(0, 0, applyTileSpin(fused, spin + time, 4));
        p.end();

        preview->setPixmap(QPixmap::fromImage(applyGoovieEffect(combo, warp, time)));
        time++;
    });

    QObject::connect(startBtn, &QPushButton::clicked, [&]() { timer->start(33); });
    QObject::connect(stopBtn, &QPushButton::clicked, [&]() { timer->stop(); });

    window.show();
    return app.exec();
}
