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
    void onRecalculateClustersClicked();


    void on_pushButtonTestQGraph_clicked();

private:
    Ui::MainWindow *ui;
    struct FileInfo
    {
        QString datasetName;
        QString fileName;
        QString extension;
        QString baseName;
        QString xmlPath;
        QStringList imagePaths;
    };
    struct XMLData
    {
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
    struct DataPoint
    {
        double x, y;
        int cluster;
        DataPoint(double x, double y) : x(x), y(y), cluster(-1) {}
    };
    struct Peak {
        int index;
        double value;
        double prominence;
    };
    struct DoseAnalysisResults {
        int    numPopulations  = 0;   // total populations found (main + outliers)
        double mainMean        = 0.0;
        double mainStd         = 0.0;
        double minSeparation   = 1.0; // parameter used
        int    keptCount       = 0;
        int    rejectedCount   = 0;
        double keptMeanDose    = 0.0;
    };

    // In mainwindow.h - add as class member:
    struct DatasetInfo {
        QString name;
        int eerCount;
        int tiffCount;
        int mrcCount;
        int jpgCount;
        int pngCount;
        int largeTiffCount;
        int smallTiffCount;
    };

    QMap<QString, DatasetInfo> datasetInfoMap;
    DoseAnalysisResults analyseAndFilterDose(QTableWidget *tableWidget, QCustomPlot *plotTotalDose,QLabel *statsLabel, double minSeparation = 1.0);
    void clusterAndRecolorBeamShifts(QCustomPlot* plot);
    QMap<QPair<double, double>, int> coordinateToCluster;
    QDir currentdir,startdir, destinationdir;
    QString extension;
    XMLData parseXMLFile(const QString& xmlFilePath);
    qint64 destinationdirectorysize;
    double meandose;
    QList<FileInfo> allFiles;
    QStringList unmatchedJPGFiles;
    QStringList unmatchedPNGFiles;
    QStringList unmatchedTIFFFiles;
    double calculateModeMAD(const QVector<double>& doses, double modeDose);

};


