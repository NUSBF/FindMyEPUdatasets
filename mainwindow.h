#include <QMainWindow>
#include <QDir>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include "qcustomplot.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButtonMoviesDirectory_clicked();
    void on_pushButtonDestinationDirectory_clicked();


    void on_pushButtonPrepareCryosparc_clicked();

private:
    Ui::MainWindow *ui;
    struct FileInfo {
        QString datasetName;
        QString fileName;
        QString extension;
        QString baseName;
        QString filePath;  // Add this field too since it's used in the move code
    };
    struct XMLData {
        QString acquisitionTime;
        QString voltage;
        QString beamshiftX;
        QString beamshiftY;
        QString beamtiltX;
        QString beamtiltY;
        QString exposureTime;
        QString pixelSize;
        QString dosePerPixel;
        QString totalDose;  // We can calculate this later
        QString movieAcquisitionRate;  // Not sure where this comes from
    };
    struct DataPoint {
        double x, y;
        int cluster;
        DataPoint(double x, double y) : x(x), y(y), cluster(-1) {}
    };

    double calculateDistance(const DataPoint& p1, const DataPoint& p2);
    QVector<QVector<DataPoint>> kMeansClustering(QVector<DataPoint>& points, int k, int maxIterations = 100);
    int autoDetectClusters(const QVector<DataPoint>& points, int maxK = 15);
    void clusterAndRecolorBeamShifts(QCustomPlot* plot, const QList<QRgb>& colorScheme, int graphIndex = 0);
    QMap<QPair<double, double>, int> coordinateToCluster;
    double findNaturalThreshold(const QVector<DataPoint>& points);
    QDir currentdir,startdir, destinationdir;
    QString extension;
    XMLData parseXMLFile(const QString& xmlFilePath);
    qint64 destinationdirectorysize;
    double meandose;
    QList<FileInfo> allFiles;
    const QList<QRgb> listrgb =
    {
        //0xFFB300,    // Vivid Yellow
        //0x803E75,    // Strong Purple
        //0xFF6800,    // Vivid Orange
        //0xA6BDD7,    // Very Light Blue
        //0xC10020,    // Vivid Red
        //0xCEA262,    // Grayish Yellow
        //0x817066,    // Medium Gray
        //0x007D34,    // Vivid Green
        //0xF6768E,    // Strong Purplish Pink
        //0x00538A,    // Strong Blue
        //0xFF7A5C,    // Strong Yellowish Pink
        //0x53377A,    // Strong Violet
        //0xFF8E00,    // Vivid Orange Yellow
        //0xB32851,    // Strong Purplish Red
        //0xF4C800,    // Vivid Greenish Yellow
        //0x7F180D,    // Strong Reddish Brown
        //0x93AA00,    // Vivid Yellowish Green
        //0x593315,    // Deep Yellowish Brown
        //0xF13A13,    // Vivid Reddish Orange
        0x330000,
        0x660000,
        0x990000,
        0xCC0000,
        0xFF0000,
        0xFF3333,
        0xFF6666,
        0xFF9999,
        0xFFCCCC,
        0x331900,
        0x663300,
        0x994C00,
        0xCC6600,
        0xFF8000,
        0xFF9933,
        0xFFB266,
        0xFFCC99,
        0xFFE5CC,
        0x333300,
        0x666600,
        0x999900,
        0xCCCC00,
        0xFFFF00,
        0xFFFF33,
        0xFFFF66,
        0xFFFF99,
        0xFFFFCC,
        0x193300,
        0x336600,
        0x4C9900,
        0x66CC00,
        0x80FF00,
        0x99FF33,
        0xB2FF66,
        0xCCFF99,
        0xE5FFCC,
        0x003300,
        0x006600,
        0x009900,
        0x00CC00,
        0x00FF00,
        0x33FF33,
        0x66FF66,
        0x99FF99,
        0xCCFFCC,
        0x003319,
        0x006633,
        0x00994C,
        0x00CC66,
        0x00FF80,
        0x33FF99,
        0x66FFB2,
        0x99FFCC,
        0xCCFFE5,
        0x003333,
        0x006666,
        0x009999,
        0x00CCCC,
        0x00FFFF,
        0x33FFFF,
        0x66FFFF,
        0x99FFFF,
        0xCCFFFF,
        0x001933,
        0x003366,
        0x004C99,
        0x0066CC,
        0x0080FF,
        0x3399FF,
        0x66B2FF,
        0x99CCFF,
        0xCCE5FF,
        0x000033,
        0x000066,
        0x000099,
        0x0000CC,
        0x0000FF,
        0x3333FF,
        0x6666FF,
        0x9999FF,
        0xCCCCFF,
        0x190033,
        0x330066,
        0x4C0099,
        0x6600CC,
        0x7F00FF,
        0x9933FF,
        0xB266FF,
        0xCC99FF,
        0xE5CCFF,
        0x330033,
        0x660066,
        0x990099,
        0xCC00CC,
        0xFF00FF,
        0xFF33FF,
        0xFF66FF,
        0xFF99FF,
        0xFFCCFF,
        0x330019,
        0x660033,
        0x99004C,
        0xCC0066,
        0xFF007F,
        0xFF3399,
        0xFF66B2,
        0xFF99CC,
        0xFFCCE5,
        0x202020,
        0x404040,
        0x606060,
        0x808080,
        0xA0A0A0,
        0xC0C0C0,
        0xE0E0E0
    };

};


