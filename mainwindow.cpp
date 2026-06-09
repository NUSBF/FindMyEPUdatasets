#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QDir>
#include <QDirIterator>
#include <QMimeDatabase>
#include <QProgressDialog>
#include <QWidget>
#include <QDebug>
#include <QElapsedTimer>
#include <QTime>
#include "dataanalysis.h"
using namespace alglib;
#include <armadillo>
#include <QtGraphs>
#include <QQuickWidget>
#include <QtGraphsWidgets>
#include <QQuick3D>
#include <QJsonObject>
#include <QJsonDocument>
#include <QPrinter>
#include <QTextDocument>
#include <QProcess>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->progressBarMovies->setValue(0);
    ui->pushButtonDestinationDirectory->setEnabled(false);

    // Initialize pixel size to magnification mapping
    pixelSizeData["1.30"].magnification = "180000";
    pixelSizeData["1.30"].calibratedPixelSize = "1.29";
    pixelSizeData["1.14883"].magnification = "230000";
    pixelSizeData["1.14883"].calibratedPixelSize = "1.13";
    pixelSizeData["0.90"].magnification = "290000";
    pixelSizeData["0.90"].calibratedPixelSize = "0.89";
    pixelSizeData["0.80"].magnification = "320000";
    pixelSizeData["0.80"].calibratedPixelSize = "1.79";

}

MainWindow::~MainWindow()
{
    delete ui;

}

MainWindow::XMLData MainWindow::parseXMLFile(const QString& xmlFilePath)
{
    XMLData data;
    QFile inputFile(xmlFilePath);

    if (inputFile.open(QIODevice::ReadOnly))
    {
        QTextStream in(&inputFile);
        while (!in.atEnd())
        {
            QString line = in.readLine();
            line.replace(">", "<");
            QStringList lineList = line.split("<");

            for (int j = 0; j < lineList.size(); j++)
            {
                if (lineList.at(j) == "acquisitionDateTime")
                {
                    data.acquisitionTime = lineList.at(j + 1);
                }

                if (lineList.at(j) == "AccelerationVoltage")
                {
                    double value = lineList.at(j + 1).toDouble();
                    value = value / 1000;
                    data.voltage = QString("%0").arg(value);
                }

                if (lineList.at(j).contains("BeamShift "))
                {
                    data.beamshiftX = lineList.at(j + 3);
                    data.beamshiftY = lineList.at(j + 7);
                }

                if (lineList.at(j).contains("BeamTilt "))
                {
                    data.beamtiltX = lineList.at(j + 3);
                    data.beamtiltY = lineList.at(j + 7);
                }

                if (lineList.at(j) == "ExposureTime")
                {
                    if (lineList.at(j + 1) != "0")
                    {
                        data.exposureTime = lineList.at(j + 1);
                    }
                }

                if (lineList.at(j) == "pixelSize")
                {
                    double value = lineList.at(j + 5).toDouble();
                    value = value * 10000000000;
                    data.pixelSize = QString("%0").arg(value);
                }

                if (lineList.at(j) == "DoseOnCamera")
                {
                    data.dosePerPixel = lineList.at(j + 4);
                }
            }
        }
        inputFile.close();
    }

    return data;
}

void MainWindow::on_pushButtonPrepareCryosparc_clicked()
{

}

double calculateModeMAD(const QVector<double>& doses, double modeDose) {
    QVector<double> absDeviations;

    for (double dose : doses) {
        absDeviations.append(qAbs(dose - modeDose));
    }

    // Return median of absolute deviations
    std::sort(absDeviations.begin(), absDeviations.end());
    return absDeviations[absDeviations.size() / 2];
}

void MainWindow::on_pushButtonTestQGraph_clicked()
{
    qDebug() << "Starting Qt Graphs test...";

    // Create QQuickWidget first
    QQuickWidget *quickWidget = new QQuickWidget();
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    // IMPORTANT: Set source format for 3D rendering
    QSurfaceFormat format = QQuick3D::idealSurfaceFormat();
    quickWidget->setFormat(format);

    // Create scatter widget item
    Q3DScatterWidgetItem *scatter = new Q3DScatterWidgetItem();
    scatter->setWidget(quickWidget);

    qDebug() << "Widget created, setting up axes...";

    // Set axis labels and ranges
    scatter->axisX()->setTitle("X Axis");
    scatter->axisX()->setRange(0, 100);
    scatter->axisY()->setTitle("Y Axis");
    scatter->axisY()->setRange(0, 50);
    scatter->axisZ()->setTitle("Z Axis");
    scatter->axisZ()->setRange(0, 10);

    qDebug() << "Adding test data...";

    // Create test series with mock data
    QScatter3DSeries *series = new QScatter3DSeries();
    series->setName("Test Data");
    series->setItemLabelFormat("X:@xLabel Y:@yLabel Z:@zLabel");
    series->setMeshSmooth(true);
    series->setBaseColor(QColor(0, 0, 255));
    series->setItemSize(0.1f);

    // Add some test points
    QScatterDataArray dataArray;
    for (int i = 0; i < 50; ++i) {
        float x = (rand() % 100);
        float y = (rand() % 50);
        float z = (rand() % 10);
        dataArray << QScatterDataItem(QVector3D(x, y, z));
    }

    series->dataProxy()->addItems(dataArray);
    scatter->addSeries(series);

    qDebug() << "Series added, showing widget...";

    quickWidget->setMinimumSize(QSize(1200, 800));
    quickWidget->show();

    qDebug() << "Widget shown";
}

void MainWindow::saveDatasetReport(const QString &datasetName, const QString &destinationPath, QWidget *graphTab, QTableWidget *datasetTable)
{
    qDebug() << "=== saveDatasetReport for" << datasetName << "===";

    // ── Navigate layout to get widgets ────────────────────────────────────
    QVBoxLayout *outerLayout = qobject_cast<QVBoxLayout*>(graphTab->layout());
    QScrollArea *scrollArea = qobject_cast<QScrollArea*>(outerLayout->itemAt(0)->widget());
    QWidget *scrollContent = scrollArea->widget();
    QVBoxLayout *scrollLayout = qobject_cast<QVBoxLayout*>(scrollContent->layout());

    QWidget *plotsRowWidget = qobject_cast<QWidget*>(scrollLayout->itemAt(0)->widget());
    QHBoxLayout *plotsRowLayout = qobject_cast<QHBoxLayout*>(plotsRowWidget->layout());

    QWidget *totalDoseWidget = qobject_cast<QWidget*>(plotsRowLayout->itemAt(0)->widget());
    QVBoxLayout *totalDoseLayout = qobject_cast<QVBoxLayout*>(totalDoseWidget->layout());
    QCustomPlot *plotTotalDose = qobject_cast<QCustomPlot*>(totalDoseLayout->itemAt(0)->widget());

    QWidget *beamShiftsWidget = qobject_cast<QWidget*>(plotsRowLayout->itemAt(1)->widget());
    QVBoxLayout *beamShiftsLayout = qobject_cast<QVBoxLayout*>(beamShiftsWidget->layout());
    QCustomPlot *plotBeamShifts = qobject_cast<QCustomPlot*>(beamShiftsLayout->itemAt(0)->widget());

    // Section 1: Sample & Grid
    QGroupBox *sec1Box = qobject_cast<QGroupBox*>(scrollLayout->itemAt(1)->widget());
    QGridLayout *sec1Layout = qobject_cast<QGridLayout*>(sec1Box->layout());
    QLineEdit *sampleNameEdit = qobject_cast<QLineEdit*>(sec1Layout->itemAtPosition(0, 1)->widget());
    QComboBox *gridTypeCombo  = qobject_cast<QComboBox*>(sec1Layout->itemAtPosition(0, 3)->widget());
    QLineEdit *waitTimeEdit   = qobject_cast<QLineEdit*>(sec1Layout->itemAtPosition(1, 1)->widget());
    QLineEdit *blotTimeEdit   = qobject_cast<QLineEdit*>(sec1Layout->itemAtPosition(1, 3)->widget());
    QLineEdit *blotForceEdit  = qobject_cast<QLineEdit*>(sec1Layout->itemAtPosition(2, 1)->widget());
    QLineEdit *humidityEdit   = qobject_cast<QLineEdit*>(sec1Layout->itemAtPosition(2, 3)->widget());
    QLineEdit *tempEdit       = qobject_cast<QLineEdit*>(sec1Layout->itemAtPosition(3, 1)->widget());

    // Section 2: Data Collection (NEW LAYOUT)
    // Row 0: Microscope (0,1), Camera (0,3)
    // Row 1: Cs (1,1), Voltage (1,3)
    // Row 2: Magnification (2,1), C2 Aperture (2,3)
    // Row 3: Objective Aperture (3,1), Instrument Pixel Size (3,3)
    // Row 4: Calibrated Pixel Size (4,1), Total Dose Requested (4,3)
    // Row 5: Total Dose Recorded (5,1), Total Movies Collected (5,3)
    // Row 6: Movies Kept (6,1), Movies Rejected (6,3)
    // Row 7: Acquisition Mode (7,1), Defocus Range (7,3)
    // Row 8: EPU Version (8,1)
    QGroupBox *sec2Box = qobject_cast<QGroupBox*>(scrollLayout->itemAt(2)->widget());
    QGridLayout *sec2Layout = qobject_cast<QGridLayout*>(sec2Box->layout());
    QComboBox *microscopeCombo  = qobject_cast<QComboBox*>(sec2Layout->itemAtPosition(0, 1)->widget());
    QComboBox *cameraCombo      = qobject_cast<QComboBox*>(sec2Layout->itemAtPosition(0, 3)->widget());
    QComboBox *csCombo          = qobject_cast<QComboBox*>(sec2Layout->itemAtPosition(1, 1)->widget());
    QComboBox *voltageCombo     = qobject_cast<QComboBox*>(sec2Layout->itemAtPosition(1, 3)->widget());
    QComboBox *magCombo         = qobject_cast<QComboBox*>(sec2Layout->itemAtPosition(2, 1)->widget());
    QComboBox *c2ApertureCombo  = qobject_cast<QComboBox*>(sec2Layout->itemAtPosition(2, 3)->widget());
    QComboBox *objApertureCombo = qobject_cast<QComboBox*>(sec2Layout->itemAtPosition(3, 1)->widget());
    QComboBox *instrumentPixelSizeCombo = qobject_cast<QComboBox*>(sec2Layout->itemAtPosition(3, 3)->widget());
    QComboBox *calibratedPixelSizeCombo = qobject_cast<QComboBox*>(sec2Layout->itemAtPosition(4, 1)->widget());
    QLineEdit *doseRequestedEdit= qobject_cast<QLineEdit*>(sec2Layout->itemAtPosition(4, 3)->widget());
    QLineEdit *doseRecordedEdit = qobject_cast<QLineEdit*>(sec2Layout->itemAtPosition(5, 1)->widget());
    QLineEdit *totalMoviesEdit  = qobject_cast<QLineEdit*>(sec2Layout->itemAtPosition(5, 3)->widget());
    QLineEdit *moviesKeptEdit   = qobject_cast<QLineEdit*>(sec2Layout->itemAtPosition(6, 1)->widget());
    QLineEdit *moviesRejectedEdit=qobject_cast<QLineEdit*>(sec2Layout->itemAtPosition(6, 3)->widget());
    QComboBox *acqModeCombo     = qobject_cast<QComboBox*>(sec2Layout->itemAtPosition(7, 1)->widget());
    QLineEdit *defocusEdit      = qobject_cast<QLineEdit*>(sec2Layout->itemAtPosition(7, 3)->widget());
    QLineEdit *epuVersionEdit   = qobject_cast<QLineEdit*>(sec2Layout->itemAtPosition(8, 1)->widget());

    // ── Read values ───────────────────────────────────────────────────────
    QString sampleName     = sampleNameEdit  ? sampleNameEdit->text()          : "";
    QString gridType       = gridTypeCombo   ? gridTypeCombo->currentText()    : "";
    QString waitTime       = waitTimeEdit    ? waitTimeEdit->text()             : "";
    QString blotTime       = blotTimeEdit    ? blotTimeEdit->text()             : "";
    QString blotForce      = blotForceEdit   ? blotForceEdit->text()            : "";
    QString humidity       = humidityEdit    ? humidityEdit->text()             : "";
    QString temperature    = tempEdit        ? tempEdit->text()                 : "";
    QString microscope     = microscopeCombo ? microscopeCombo->currentText()  : "";
    QString camera         = cameraCombo     ? cameraCombo->currentText()      : "";
    QString cs             = csCombo         ? csCombo->currentText()          : "";
    QString voltage        = voltageCombo    ? voltageCombo->currentText()      : "";
    QString magnification  = magCombo        ? magCombo->currentText()         : "";
    QString c2Aperture     = c2ApertureCombo ? c2ApertureCombo->currentText()  : "";
    QString objAperture    = objApertureCombo? objApertureCombo->currentText() : "";
    QString instrumentPixelSize = instrumentPixelSizeCombo ? instrumentPixelSizeCombo->currentText() : "";
    QString calibratedPixelSize = calibratedPixelSizeCombo ? calibratedPixelSizeCombo->currentText() : "";
    QString doseRequested  = doseRequestedEdit? doseRequestedEdit->text()       : "";
    QString doseRecorded   = doseRecordedEdit ? doseRecordedEdit->text()        : "";
    QString totalMovies    = totalMoviesEdit  ? totalMoviesEdit->text()         : "";
    QString moviesKept     = moviesKeptEdit   ? moviesKeptEdit->text()          : "";
    QString moviesRejected = moviesRejectedEdit? moviesRejectedEdit->text()     : "";
    QString acqMode        = acqModeCombo    ? acqModeCombo->currentText()     : "";
    QString defocusRange   = defocusEdit     ? defocusEdit->text()              : "";
    QString epuVersion     = epuVersionEdit  ? epuVersionEdit->text()           : "";

    // ── Save plots ────────────────────────────────────────────────────────
    QString dosePlotPath = destinationPath + "/total_dose_plot.png";
    QString beamPlotPath = destinationPath + "/beam_shifts_plot.png";
    if (plotTotalDose)  plotTotalDose->savePng(dosePlotPath, 1200, 800, 1.0, -1);
    if (plotBeamShifts) plotBeamShifts->savePng(beamPlotPath, 1200, 800, 1.0, -1);

    QString generatedDate = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    // ── CSV ───────────────────────────────────────────────────────────────
    {
        QFile f(destinationPath + "/dataset_report.csv");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream s(&f);
            s << "Section,Parameter,Value\n";
            s << "Sample & Grid,Sample Name," << sampleName << "\n";
            s << "Sample & Grid,Grid Type," << gridType << "\n";
            s << "Sample & Grid,Waiting Time Before Blotting (s)," << waitTime << "\n";
            s << "Sample & Grid,Blotting Time (s)," << blotTime << "\n";
            s << "Sample & Grid,Blotting Force," << blotForce << "\n";
            s << "Sample & Grid,Humidity (%)," << humidity << "\n";
            s << "Sample & Grid,Temperature (C)," << temperature << "\n";
            s << "Data Collection,Microscope," << microscope << "\n";
            s << "Data Collection,Camera," << camera << "\n";
            s << "Data Collection,Cs (mm)," << cs << "\n";
            s << "Data Collection,Voltage (kV)," << voltage << "\n";
            s << "Data Collection,Magnification," << magnification << "\n";
            s << "Data Collection,C2 Aperture (um)," << c2Aperture << "\n";
            s << "Data Collection,Objective Aperture (um)," << objAperture << "\n";
            s << "Data Collection,Instrument Pixel Size (A/px)," << instrumentPixelSize << "\n";
            s << "Data Collection,Calibrated Pixel Size (A/px)," << calibratedPixelSize << "\n";
            s << "Data Collection,Acquisition Mode," << acqMode << "\n";
            s << "Data Collection,Total Dose Requested (e/A2)," << doseRequested << "\n";
            s << "Data Collection,Total Dose Recorded (e/A2)," << doseRecorded << "\n";
            s << "Data Collection,Total Movies Collected," << totalMovies << "\n";
            s << "Data Collection,Movies Kept," << moviesKept << "\n";
            s << "Data Collection,Movies Rejected," << moviesRejected << "\n";
            s << "Data Collection,Defocus Range (um)," << defocusRange << "\n";
            s << "Data Collection,EPU Version," << epuVersion << "\n";
            f.close();
            qDebug() << "  CSV saved";
        }
    }

    // ── JSON ──────────────────────────────────────────────────────────────
    {
        QJsonObject root;
        root["generated"] = generatedDate;
        root["dataset"]   = datasetName;

        QJsonObject sec1Obj;
        sec1Obj["sampleName"]  = sampleName;
        sec1Obj["gridType"]    = gridType;
        sec1Obj["waitTime"]    = waitTime;
        sec1Obj["blotTime"]    = blotTime;
        sec1Obj["blotForce"]   = blotForce;
        sec1Obj["humidity"]    = humidity;
        sec1Obj["temperature"] = temperature;
        root["sampleAndGrid"]  = sec1Obj;

        QJsonObject sec2Obj;
        sec2Obj["microscope"]     = microscope;
        sec2Obj["camera"]         = camera;
        sec2Obj["cs"]             = cs;
        sec2Obj["voltage"]        = voltage;
        sec2Obj["magnification"]  = magnification;
        sec2Obj["c2Aperture"]     = c2Aperture;
        sec2Obj["objAperture"]    = objAperture;
        sec2Obj["instrumentPixelSize"] = instrumentPixelSize;
        sec2Obj["calibratedPixelSize"] = calibratedPixelSize;
        sec2Obj["acqMode"]        = acqMode;
        sec2Obj["doseRequested"]  = doseRequested;
        sec2Obj["doseRecorded"]   = doseRecorded;
        sec2Obj["totalMovies"]    = totalMovies;
        sec2Obj["moviesKept"]     = moviesKept;
        sec2Obj["moviesRejected"] = moviesRejected;
        sec2Obj["defocusRange"]   = defocusRange;
        sec2Obj["epuVersion"]     = epuVersion;
        root["dataCollection"]    = sec2Obj;

        QFile f(destinationPath + "/dataset_report.json");
        if (f.open(QIODevice::WriteOnly))
        {
            f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            f.close();
            qDebug() << "  JSON saved";
        }
    }

    // ── HTML ──────────────────────────────────────────────────────────────
    {
        auto imgToBase64 = [](const QString &path) -> QString {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) return "";
            return f.readAll().toBase64();
        };
        QString doseB64 = imgToBase64(dosePlotPath);
        QString beamB64 = imgToBase64(beamPlotPath);

        QString html;
        html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
        html += "<title>CryoEM Data Collection Report - " + datasetName + "</title>";
        html += "<style>";
        html += "body{font-family:Arial,sans-serif;font-size:12px;margin:20mm;color:#222;}";
        html += "h1{font-size:16px;color:#003399;text-align:center;margin-bottom:4px;}";
        html += "h2{font-size:11px;color:#003399;border-bottom:1px solid #003399;margin-top:12px;margin-bottom:4px;}";
        html += "table{width:100%;border-collapse:collapse;margin-bottom:8px;}";
        html += "th{background:#003399;color:white;font-size:10px;padding:3px 6px;text-align:left;}";
        html += "td{font-size:10px;padding:3px 6px;border:1px solid #ccc;}";
        html += "tr:nth-child(even) td{background:#f0f4ff;}";
        html += ".plots{display:flex;gap:10px;margin-top:8px;}";
        html += ".plots img{width:49%;border:1px solid #ccc;}";
        html += ".footer{font-size:8px;color:#888;text-align:center;margin-top:16px;border-top:1px solid #ccc;padding-top:4px;}";
        html += "</style></head><body>";
        html += "<h1>CryoEM Data Collection Report<br><span style='font-size:13px;font-weight:normal;'>" + datasetName + "</span></h1>";
        html += "<p style='text-align:center;font-size:9px;color:#888;'>Newcastle University Structural Biology Facility &bull; Generated " + generatedDate + "</p>";
        html += "<h2>1. Sample &amp; Grid Information</h2>";
        html += "<table><tr><th>Parameter</th><th>Value</th><th>Parameter</th><th>Value</th></tr>";
        html += "<tr><td>Sample Name</td><td>" + sampleName + "</td><td>Grid Type</td><td>" + gridType + "</td></tr>";
        html += "<tr><td>Waiting Time Before Blotting (s)</td><td>" + waitTime + "</td><td>Blotting Time (s)</td><td>" + blotTime + "</td></tr>";
        html += "<tr><td>Blotting Force</td><td>" + blotForce + "</td><td>Humidity (%)</td><td>" + humidity + "</td></tr>";
        html += "<tr><td>Temperature (C)</td><td>" + temperature + "</td><td></td><td></td></tr>";
        html += "</table>";
        html += "<h2>2. Data Collection Parameters</h2>";
        html += "<table><tr><th>Parameter</th><th>Value</th><th>Parameter</th><th>Value</th></tr>";
        html += "<tr><td>Microscope</td><td>" + microscope + "</td><td>Camera</td><td>" + camera + "</td></tr>";
        html += "<tr><td>Cs (mm)</td><td>" + cs + "</td><td>Voltage (kV)</td><td>" + voltage + "</td></tr>";
        html += "<tr><td>Magnification</td><td>" + magnification + "</td><td>C2 Aperture (um)</td><td>" + c2Aperture + "</td></tr>";
        html += "<tr><td>Objective Aperture (um)</td><td>" + objAperture + "</td><td>Instrument Pixel Size (A/px)</td><td>" + instrumentPixelSize + "</td></tr>";
        html += "<tr><td>Calibrated Pixel Size (A/px)</td><td>" + calibratedPixelSize + "</td><td>Acquisition Mode</td><td>" + acqMode + "</td></tr>";
        html += "<tr><td>Total Dose Requested (e/A2)</td><td>" + doseRequested + "</td><td>Total Dose Recorded (e/A2)</td><td>" + doseRecorded + "</td></tr>";
        html += "<tr><td>Total Movies Collected</td><td>" + totalMovies + "</td><td>Movies Kept</td><td>" + moviesKept + "</td></tr>";
        html += "<tr><td>Movies Rejected</td><td>" + moviesRejected + "</td><td>Defocus Range (um)</td><td>" + defocusRange + "</td></tr>";
        html += "<tr><td>EPU Version</td><td>" + epuVersion + "</td><td></td><td></td></tr>";
        html += "</table>";
        html += "<h2>3. Total Dose Analysis &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; 4. Beam Shift Clustering</h2>";
        html += "<div class='plots'>";
        if (!doseB64.isEmpty())
            html += "<img src='data:image/png;base64," + doseB64 + "' alt='Total Dose Plot'/>";
        else
            html += "<div style='width:49%;border:1px solid #ccc;height:200px;background:#eee;text-align:center;'>Plot not available</div>";
        if (!beamB64.isEmpty())
            html += "<img src='data:image/png;base64," + beamB64 + "' alt='Beam Shift Plot'/>";
        else
            html += "<div style='width:49%;border:1px solid #ccc;height:200px;background:#eee;text-align:center;'>Plot not available</div>";
        html += "</div>";
        html += "<div class='footer'>Generated by FindMyEPUdatasets &bull; Newcastle University Structural Biology Facility &bull; nusbf.ncl.ac.uk</div>";
        html += "</body></html>";

        QFile f(destinationPath + "/dataset_report.html");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream s(&f);
            s << html;
            f.close();
            qDebug() << "  HTML saved";
        }
    }

    // ── PDF ───────────────────────────────────────────────────────────────
    {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(destinationPath + "/dataset_report.pdf");
        printer.setPageSize(QPageSize(QPageSize::A4));
        printer.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);

        QTextDocument doc;
        doc.setDefaultStyleSheet(
            "body{font-family:Arial;font-size:9pt;}"
            "h1{font-size:13pt;color:#003399;text-align:center;margin-bottom:2px;}"
            "h2{font-size:9pt;color:#003399;border-bottom:1px solid #003399;margin-top:6px;margin-bottom:3px;}"
            "table{border-collapse:collapse;width:100%;margin-bottom:4px;}"
            "th{background-color:#003399;color:white;padding:2px;font-size:8pt;}"
            "td{border:1px solid #ccc;padding:2px;font-size:8pt;}"
            ".plots{text-align:center;}"
            ".plots img{width:45%;margin:2px;}"
            );

        QString pdfHtml;
        pdfHtml += "<h1>CryoEM Data Collection Report<br/><span style='font-size:11pt;font-weight:normal;'>" + datasetName + "</span></h1>";
        pdfHtml += "<p style='text-align:center;font-size:7pt;color:grey;margin:2px;'>Newcastle University Structural Biology Facility &bull; Generated " + generatedDate + "</p>";
        pdfHtml += "<h2>1. Sample &amp; Grid Information</h2>";
        pdfHtml += "<table><tr><th>Parameter</th><th>Value</th><th>Parameter</th><th>Value</th></tr>";
        pdfHtml += "<tr><td>Sample Name</td><td>" + sampleName + "</td><td>Grid Type</td><td>" + gridType + "</td></tr>";
        pdfHtml += "<tr><td>Waiting Time Before Blotting (s)</td><td>" + waitTime + "</td><td>Blotting Time (s)</td><td>" + blotTime + "</td></tr>";
        pdfHtml += "<tr><td>Blotting Force</td><td>" + blotForce + "</td><td>Humidity (%)</td><td>" + humidity + "</td></tr>";
        pdfHtml += "<tr><td>Temperature (C)</td><td>" + temperature + "</td><td></td><td></td></tr>";
        pdfHtml += "</table>";
        pdfHtml += "<h2>2. Data Collection Parameters</h2>";
        pdfHtml += "<table><tr><th>Parameter</th><th>Value</th><th>Parameter</th><th>Value</th></tr>";
        pdfHtml += "<tr><td>Microscope</td><td>" + microscope + "</td><td>Camera</td><td>" + camera + "</td></tr>";
        pdfHtml += "<tr><td>Cs (mm)</td><td>" + cs + "</td><td>Voltage (kV)</td><td>" + voltage + "</td></tr>";
        pdfHtml += "<tr><td>Magnification</td><td>" + magnification + "</td><td>C2 Aperture (um)</td><td>" + c2Aperture + "</td></tr>";
        pdfHtml += "<tr><td>Objective Aperture (um)</td><td>" + objAperture + "</td><td>Instrument Pixel Size</td><td>" + instrumentPixelSize + "</td></tr>";
        pdfHtml += "<tr><td>Calibrated Pixel Size</td><td>" + calibratedPixelSize + "</td><td>Acquisition Mode</td><td>" + acqMode + "</td></tr>";
        pdfHtml += "<tr><td>Dose Requested (e/A2)</td><td>" + doseRequested + "</td><td>Dose Recorded (e/A2)</td><td>" + doseRecorded + "</td></tr>";
        pdfHtml += "<tr><td>Total Movies</td><td>" + totalMovies + "</td><td>Movies Kept</td><td>" + moviesKept + "</td></tr>";
        pdfHtml += "<tr><td>Movies Rejected</td><td>" + moviesRejected + "</td><td>Defocus Range (um)</td><td>" + defocusRange + "</td></tr>";
        pdfHtml += "<tr><td>EPU Version</td><td>" + epuVersion + "</td><td></td><td></td></tr>";
        pdfHtml += "</table>";
        pdfHtml += "<h2 style='text-align:center;'>3. Total Dose Analysis / 4. Beam Shift Clustering</h2>";
        pdfHtml += "<div class='plots'>";
        if (QFile::exists(dosePlotPath))
            pdfHtml += "<img src='" + dosePlotPath + "' width='350'/>";
        if (QFile::exists(beamPlotPath))
            pdfHtml += "<img src='" + beamPlotPath + "' width='350'/>";
        pdfHtml += "</div>";

        doc.setHtml(pdfHtml);
        doc.print(&printer);
        qDebug() << "  PDF saved";
    }

    // ── DOCX ──────────────────────────────────────────────────────────────
    {
        auto xmlEscape = [](const QString &s) -> QString {
            QString r = s;
            r.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;").replace("\"","&quot;");
            return r;
        };

        auto makeRow = [&](const QString &a, const QString &b, const QString &c, const QString &d, bool header = false) -> QString {
            QString shading = header ? "<w:tcPr><w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"003399\"/></w:tcPr>" : "";
            QString style   = header ? "<w:rPr><w:b/><w:color w:val=\"FFFFFF\"/></w:rPr>" : "";
            QString r = "<w:tr>";
            for (const QString &cell : {a, b, c, d})
                r += "<w:tc>" + shading + "<w:p><w:r>" + style + "<w:t>" + xmlEscape(cell) + "</w:t></w:r></w:p></w:tc>";
            r += "</w:tr>";
            return r;
        };

        QString body;
        body += "<w:p><w:pPr><w:jc w:val=\"center\"/></w:pPr><w:r><w:rPr><w:b/><w:sz w:val=\"28\"/><w:color w:val=\"003399\"/></w:rPr><w:t>CryoEM Data Collection Report</w:t></w:r></w:p>";
        body += "<w:p><w:pPr><w:jc w:val=\"center\"/></w:pPr><w:r><w:rPr><w:sz w:val=\"22\"/></w:rPr><w:t>" + xmlEscape(datasetName) + "</w:t></w:r></w:p>";
        body += "<w:p><w:pPr><w:jc w:val=\"center\"/></w:pPr><w:r><w:rPr><w:sz w:val=\"16\"/><w:color w:val=\"888888\"/></w:rPr><w:t>Newcastle University Structural Biology Facility - Generated " + xmlEscape(generatedDate) + "</w:t></w:r></w:p>";
        body += "<w:p><w:r><w:rPr><w:b/><w:color w:val=\"003399\"/></w:rPr><w:t>1. Sample &amp; Grid Information</w:t></w:r></w:p>";
        body += "<w:tbl><w:tblPr><w:tblW w:w=\"9000\" w:type=\"dxa\"/></w:tblPr>";
        body += makeRow("Parameter","Value","Parameter","Value", true);
        body += makeRow("Sample Name", sampleName, "Grid Type", gridType);
        body += makeRow("Waiting Time Before Blotting (s)", waitTime, "Blotting Time (s)", blotTime);
        body += makeRow("Blotting Force", blotForce, "Humidity (%)", humidity);
        body += makeRow("Temperature (C)", temperature, "", "");
        body += "</w:tbl><w:p/>";
        body += "<w:p><w:r><w:rPr><w:b/><w:color w:val=\"003399\"/></w:rPr><w:t>2. Data Collection Parameters</w:t></w:r></w:p>";
        body += "<w:tbl><w:tblPr><w:tblW w:w=\"9000\" w:type=\"dxa\"/></w:tblPr>";
        body += makeRow("Parameter","Value","Parameter","Value", true);
        body += makeRow("Microscope", microscope, "Camera", camera);
        body += makeRow("Cs (mm)", cs, "Voltage (kV)", voltage);
        body += makeRow("Magnification", magnification, "C2 Aperture (um)", c2Aperture);
        body += makeRow("Objective Aperture (um)", objAperture, "Instrument Pixel Size (A/px)", instrumentPixelSize);
        body += makeRow("Calibrated Pixel Size (A/px)", calibratedPixelSize, "Acquisition Mode", acqMode);
        body += makeRow("Total Dose Requested (e/A2)", doseRequested, "Total Dose Recorded (e/A2)", doseRecorded);
        body += makeRow("Total Movies Collected", totalMovies, "Movies Kept", moviesKept);
        body += makeRow("Movies Rejected", moviesRejected, "Defocus Range (um)", defocusRange);
        body += makeRow("EPU Version", epuVersion, "", "");
        body += "</w:tbl><w:p/>";
        body += "<w:p><w:r><w:rPr><w:b/><w:color w:val=\"003399\"/></w:rPr><w:t>3. Total Dose Analysis / 4. Beam Shift Clustering</w:t></w:r></w:p>";
        body += "<w:p><w:r><w:rPr><w:i/><w:color w:val=\"888888\"/></w:rPr><w:t>See total_dose_plot.png and beam_shifts_plot.png in this folder.</w:t></w:r></w:p>";

        QString docXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
            "<w:body>" + body + "</w:body></w:document>";

        QString relsXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
            "</Relationships>";

        QString wordRelsXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "</Relationships>";

        QString contentTypesXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
            "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
            "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
            "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
            "</Types>";

        QString tmpDir = destinationPath + "/_docxtmp";
        QDir().mkpath(tmpDir + "/_rels");
        QDir().mkpath(tmpDir + "/word/_rels");

        auto writeFile = [](const QString &path, const QString &content) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text))
                f.write(content.toUtf8());
        };

        writeFile(tmpDir + "/[Content_Types].xml", contentTypesXml);
        writeFile(tmpDir + "/_rels/.rels", relsXml);
        writeFile(tmpDir + "/word/document.xml", docXml);
        writeFile(tmpDir + "/word/_rels/document.xml.rels", wordRelsXml);

        QString docxPath = destinationPath + "/dataset_report.docx";
        QFile::remove(docxPath);

        QProcess zip;
        zip.setWorkingDirectory(tmpDir);
        zip.start("zip", {"-r", docxPath, "."});
        zip.waitForFinished(10000);

        QDir(tmpDir).removeRecursively();
        qDebug() << "  DOCX saved";
    }

    qDebug() << "=== saveDatasetReport complete ===";
}

void MainWindow::clusterAndRecolorBeamShifts(QCustomPlot* plot)
{
    QElapsedTimer timer;
    timer.start();

    QCPCurve *beamCurve = plot->findChild<QCPCurve*>("beamCurve");
    if (!beamCurve || beamCurve->data()->isEmpty()) return;

    // --- 1. Extraction & Normalization ---
    int n = beamCurve->data()->size();
    QVector<double> xs, ys;
    xs.reserve(n); ys.reserve(n);
    double xMin = 1e18, xMax = -1e18, yMin = 1e18, yMax = -1e18;

    for (auto it = beamCurve->data()->begin(); it != beamCurve->data()->end(); ++it) {
        xs.append(it->key); ys.append(it->value);
        xMin = std::min(xMin, it->key); xMax = std::max(xMax, it->key);
        yMin = std::min(yMin, it->value); yMax = std::max(yMax, it->value);
    }

    double maxRange = std::max(xMax - xMin, yMax - yMin);
    if (maxRange <= 0) return;

    // --- 2. Grid Density Mapping ---
    const int G = qMax(8, qMin(64, (int)std::sqrt((double)n / 10)));
    QVector<QVector<int>> grid(G, QVector<int>(G, 0));
    QVector<QVector<QVector<int>>> cellPoints(G, QVector<QVector<int>>(G));

    for (int i = 0; i < n; ++i) {
        int gx = std::clamp(static_cast<int>((xs[i] - xMin) / maxRange * (G - 1)), 0, G - 1);
        int gy = std::clamp(static_cast<int>((ys[i] - yMin) / maxRange * (G - 1)), 0, G - 1);
        grid[gx][gy]++;
        cellPoints[gx][gy].append(i);
    }

    // --- 3. Peak Finding & CoM Refinement ---
    struct ClusterAnchor { QPointF center; int id; };
    QVector<ClusterAnchor> anchors;

    for (int x = 1; x < G - 1; ++x) {
        for (int y = 1; y < G - 1; ++y) {
            int val = grid[x][y];
            if (val <= 1) continue;

            // Strict local maxima check (8 neighbours)
            if (val > grid[x-1][y-1] && val > grid[x][y-1] && val > grid[x+1][y-1] &&
                val > grid[x-1][y]   && val > grid[x+1][y]  &&
                val > grid[x-1][y+1] && val > grid[x][y+1] && val > grid[x+1][y+1])
            {
                // Calculate CoM for this peak using local 3x3 window
                double sumX = 0, sumY = 0;
                int totalPoints = 0;
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int idx : cellPoints[x + dx][y + dy]) {
                            sumX += xs[idx]; sumY += ys[idx];
                            totalPoints++;
                        }
                    }
                }
                if (totalPoints > 0)
                    anchors.append({QPointF(sumX / totalPoints, sumY / totalPoints), (int)anchors.size()});
            }
        }
    }

    // --- 4. Point Assignment ---
    QVector<int> labels(n, -1);
    for (int i = 0; i < n; ++i) {
        double minD2 = std::numeric_limits<double>::max();
        int bestID = -1;
        for (const auto& anchor : anchors) {
            double d2 = std::pow(xs[i] - anchor.center.x(), 2) + std::pow(ys[i] - anchor.center.y(), 2);
            if (d2 < minD2) { minD2 = d2; bestID = anchor.id; }
        }
        labels[i] = bestID;
    }

    // --- 5. Populate coordinateToCluster for table column 23 ---
    coordinateToCluster.clear();
    for (int i = 0; i < n; ++i)
        if (labels[i] >= 0)
            coordinateToCluster[QPair<double,double>(xs[i], ys[i])] = labels[i] + 1;

    // --- 6. Debug Logging ---
    qDebug() << "--- Grid-CoM Debug ---";
    qDebug() << "Raw Points:" << n;
    qDebug() << "Grid size:" << G << "x" << G;
    qDebug() << "X Range:" << xMin << "to" << xMax;
    qDebug() << "Y Range:" << yMin << "to" << yMax;
    qDebug() << "Peaks/Clusters Found:" << anchors.size();
    qDebug() << "Execution Time:" << timer.elapsed() << "ms";

    // --- 7. Cleanup old cluster curves and labels ---
    for (int i = plot->plottableCount() - 1; i >= 0; --i)
        if (plot->plottable(i)->objectName().startsWith("clusterCurve_"))
            plot->removePlottable(i);
    for (int i = plot->itemCount() - 1; i >= 0; --i)
        if (qobject_cast<QCPItemText*>(plot->item(i)))
            plot->removeItem(i);

    beamCurve->setVisible(false);

    // --- 8. Create one curve per cluster ---
    int clusterCount = anchors.size();
    QVector<QCPCurve*> curves(clusterCount);
    QVector<int> count(clusterCount, 0);

    for (int c = 0; c < clusterCount; ++c) {
        curves[c] = new QCPCurve(plot->xAxis, plot->yAxis);
        curves[c]->setObjectName(QString("clusterCurve_%1").arg(c));
        // Golden angle colour distribution: maximises perceptual distance between neighbours
        double hue = fmod(c * 137.508, 360.0);
        double sat = (c % 2 == 0) ? 0.85 : 0.60;
        double val = (c % 3 == 0) ? 0.95 : (c % 3 == 1) ? 0.75 : 0.55;
        QColor col = QColor::fromHsvF(hue / 360.0, sat, val);
        curves[c]->setPen(QPen(col, 2));
        curves[c]->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCross, col, col, 8));
        curves[c]->setLineStyle(QCPCurve::lsNone);
    }

    // --- 9. Assign points to curves ---
    for (int i = 0; i < n; ++i) {
        int cid = labels[i];
        if (cid >= 0) curves[cid]->addData(count[cid]++, xs[i], ys[i]);
    }

    // --- 10. Cluster centre labels ---
    for (int c = 0; c < clusterCount; ++c) {
        if (count[c] == 0) continue;
        QCPItemText *lbl = new QCPItemText(plot);
        lbl->setPositionAlignment(Qt::AlignCenter);
        lbl->position->setType(QCPItemPosition::ptPlotCoords);
        lbl->position->setCoords(anchors[c].center.x(), anchors[c].center.y());
        lbl->setText(QString::number(c + 1));
        lbl->setFont(QFont("Arial", 10, QFont::Bold));
        lbl->setBrush(QBrush(Qt::white));
        lbl->setPen(QPen(Qt::black));
        lbl->setPadding(QMargins(2, 2, 2, 2));
    }

    plot->rescaleAxes();
    plot->replot();

    qDebug() << "TOTAL TIME:" << timer.elapsed() << "ms";
}

void MainWindow::on_pushButtonDestinationDirectory_clicked()
{
    int totalOperations = 0;
    for (int i = 0; i < ui->tableWidgetDatasets->rowCount(); ++i)
    {
        QCheckBox *checkbox = qobject_cast<QCheckBox*>(ui->tableWidgetDatasets->cellWidget(i, 0));
        if (checkbox && checkbox->isChecked())
        {
            int matchedFiles = ui->tableWidgetDatasets->item(i, 3)->text().toInt();
            totalOperations += matchedFiles * 3;
        }
    }

    ui->progressBarMovies->setMaximum(totalOperations);
    ui->progressBarMovies->setValue(0);

    int currentOperation = 0;
    QElapsedTimer totalTimer;
    totalTimer.start();

    qDebug() << "=== STARTING DATASET EXPORT PROCESS ===";
    qDebug() << "Step 1: Getting destination directory...";
    QElapsedTimer stepTimer;
    stepTimer.start();

    QFileDialog dialog(this);
    dialog.setOptions(QFileDialog::HideNameFilterDetails | QFileDialog::DontUseNativeDialog);
    dialog.setDirectory("/home/data/raw4/2026/");
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setLabelText(QFileDialog::FileName,"Folder");
    QStringList ListOfFiles;
    if (dialog.exec())
    {
        ListOfFiles=dialog.selectedFiles();
    }
    else
    {
        qDebug() << "Step 1: CANCELLED - User cancelled directory selection";
        return;
    }

    QString destinationPath = ListOfFiles[0];
    qDebug() << "Step 1: COMPLETED in" << stepTimer.elapsed() << "ms - Destination:" << destinationPath;

    QElapsedTimer timer;
    timer.start();
    QApplication::setOverrideCursor(Qt::WaitCursor);

    stepTimer.restart();
    qDebug() << "Step 2: Getting selected datasets from table...";
    QStringList selectedDatasets;
    for (int i = 0; i < ui->tableWidgetDatasets->rowCount(); ++i)
    {
        QCheckBox *checkbox = qobject_cast<QCheckBox*>(ui->tableWidgetDatasets->cellWidget(i, 0));
        if (checkbox && checkbox->isChecked())
        {
            QString datasetName = ui->tableWidgetDatasets->item(i, 1)->text();
            selectedDatasets.append(datasetName);
        }
    }

    if (selectedDatasets.isEmpty())
    {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, "No Selection", "Please select at least one dataset.");
        qDebug() << "Step 2: FAILED - No datasets selected";
        return;
    }

    if (allFiles.isEmpty())
    {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, "No Data", "No file data available. Please scan directories first.");
        qDebug() << "Step 2: FAILED - No file data available";
        return;
    }
    qDebug() << "Step 2: COMPLETED in" << stepTimer.elapsed() << "ms - Selected" << selectedDatasets.size() << "datasets:" << selectedDatasets;

    stepTimer.restart();
    qDebug() << "Step 3: Getting rename information for datasets...";
    QMap<QString, QString> datasetRenames;
    for (const QString& dataset : selectedDatasets)
    {
        QApplication::restoreOverrideCursor();
        qDebug() << "  Getting new name for dataset:" << dataset;
        QDialog renameDialog(this);
        renameDialog.setWindowTitle("Rename Dataset");
        renameDialog.setMinimumWidth(qMax(400, dataset.length() * 8));

        QVBoxLayout *layout = new QVBoxLayout(&renameDialog);
        QLabel *label = new QLabel(QString("Enter new name for dataset:\n%1").arg(dataset));
        QLineEdit *lineEdit = new QLineEdit(dataset);
        lineEdit->selectAll();

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

        layout->addWidget(label);
        layout->addWidget(lineEdit);
        layout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, &renameDialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &renameDialog, &QDialog::reject);

        if (renameDialog.exec() != QDialog::Accepted)
        {
            qDebug() << "Step 3: CANCELLED - User cancelled rename for dataset:" << dataset;
            return;
        }

        datasetRenames[dataset] = lineEdit->text();
        qDebug() << "  Renamed:" << dataset << "->" << lineEdit->text();
        QApplication::setOverrideCursor(Qt::WaitCursor);
    }
    qDebug() << "Step 3: COMPLETED in" << stepTimer.elapsed() << "ms - All datasets renamed";

    stepTimer.restart();
    qDebug() << "Step 4: Processing datasets and organizing rows by peak...";

    QMap<QString, QMap<int, QList<int>>> rowNumbersPerPeak;
    QMap<QString, QString> datasetRawType;
    QMap<QString, QTableWidget*> datasetTables;

    for (const QString& originalDataset : selectedDatasets)
    {
        qDebug() << "  Processing dataset:" << originalDataset;

        QTableWidget *datasetTable = nullptr;
        for (int i = 0; i < ui->tabWidgetFiles->count(); ++i)
        {
            if (ui->tabWidgetFiles->tabText(i) == originalDataset)
            {
                QWidget *tableTab = ui->tabWidgetFiles->widget(i);
                QVBoxLayout *tableLayout = qobject_cast<QVBoxLayout*>(tableTab->layout());
                if (tableLayout && tableLayout->count() >= 1)
                    datasetTable = qobject_cast<QTableWidget*>(tableLayout->itemAt(0)->widget());
                break;
            }
        }

        if (!datasetTable)
        {
            qDebug() << "    ERROR: Could not find table for dataset:" << originalDataset;
            continue;
        }

        datasetTables[originalDataset] = datasetTable;
        qDebug() << "    Reading table with" << datasetTable->rowCount() << "rows...";

        int eerCount = 0, tiffCount = 0, mrcCount = 0;

        for (int row = 0; row < datasetTable->rowCount(); ++row)
        {
            QString eerExists  = datasetTable->item(row, 3)->text();
            QString tiffExists = datasetTable->item(row, 4)->text();
            QString mrcExists  = datasetTable->item(row, 5)->text();

            if (eerExists  == "Yes") eerCount++;
            if (tiffExists == "Yes") tiffCount++;
            if (mrcExists  == "Yes") mrcCount++;

            QTableWidgetItem *keepItem = datasetTable->item(row, 25);
            if (keepItem && keepItem->text() == "Yes")
            {
                int peakNumber = datasetTable->item(row, 24)->text().toInt();
                rowNumbersPerPeak[originalDataset][peakNumber].append(row);
            }
        }

        if (eerCount > 0)       datasetRawType[originalDataset] = "EER";
        else if (tiffCount > 0) datasetRawType[originalDataset] = "TIFF";
        else if (mrcCount > 0)  datasetRawType[originalDataset] = "MRC";

        qDebug() << "    Raw data type:" << datasetRawType[originalDataset];
        qDebug() << "    Dataset" << originalDataset << "organized into" << rowNumbersPerPeak[originalDataset].size() << "peaks";
        for (auto it = rowNumbersPerPeak[originalDataset].constBegin(); it != rowNumbersPerPeak[originalDataset].constEnd(); ++it)
            qDebug() << "      Peak" << it.key() << ":" << it.value().size() << "rows";
    }

    qDebug() << "Step 4: COMPLETED in" << stepTimer.elapsed() << "ms";

    stepTimer.restart();
    qDebug() << "Step 5: create folder structure";

    for (const QString& originalDataset : selectedDatasets)
    {
        QString newDatasetName = datasetRenames[originalDataset];
        QString baseDatasetDir = destinationPath + "/" + newDatasetName;
        QString rawType = datasetRawType[originalDataset].toLower();

        if (rowNumbersPerPeak[originalDataset].size() > 1)
        {
            for (auto it = rowNumbersPerPeak[originalDataset].constBegin(); it != rowNumbersPerPeak[originalDataset].constEnd(); ++it)
            {
                int peak = it.key();
                QDir().mkpath(baseDatasetDir + QString("/peak%1/data/movies").arg(peak));
                QDir().mkpath(baseDatasetDir + QString("/peak%1/data/xml").arg(peak));
                QDir().mkpath(baseDatasetDir + QString("/peak%1/data/raw/%2").arg(peak).arg(rawType));
            }
        }
        else
        {
            QDir().mkpath(baseDatasetDir + "/data/movies");
            QDir().mkpath(baseDatasetDir + "/data/xml");
            QDir().mkpath(baseDatasetDir + "/data/raw/" + rawType);
        }
    }

    qDebug() << "Step 5: COMPLETED in" << stepTimer.elapsed() << "ms";

    stepTimer.restart();
    qDebug() << "Step 6: Searching for support files...";

    QString sourceDir = ui->labelSelectedDirectoryMovie->text();
    qDebug() << "  Source directory:" << sourceDir;

    QString glaciosFile = "";
    bool glaciosFileExists = false;
    qDebug() << "  Searching for Glacios_Data_Collection_Parameters.txt...";
    QDirIterator glaciosIterator(sourceDir, QStringList() << "Glacios_Data_Collection_Parameters.txt", QDir::Files, QDirIterator::Subdirectories);
    if (glaciosIterator.hasNext())
    {
        glaciosIterator.next();
        glaciosFile = glaciosIterator.filePath();
        glaciosFileExists = true;
        qDebug() << "  Found Glacios file:" << glaciosFile;
    }
    else
        qDebug() << "  Glacios file not found";

    QString gainFile = "";
    bool gainFileExists = false;
    qDebug() << "  Searching for *.gain files...";
    QDirIterator gainIterator(sourceDir, QStringList() << "*.gain", QDir::Files, QDirIterator::Subdirectories);
    if (gainIterator.hasNext())
    {
        gainIterator.next();
        gainFile = gainIterator.filePath();
        gainFileExists = true;
        qDebug() << "  Found gain file:" << gainFile;
    }
    else
        qDebug() << "  Gain file not found";

    qDebug() << "Step 6: COMPLETED in" << stepTimer.elapsed() << "ms";

    stepTimer.restart();
    qDebug() << "Step 7: Copy unmatched files and support files";

    int totalFilesCopied = 0;
    qint64 totalBytesCopied = 0;

    for (const QString& originalDataset : selectedDatasets)
    {
        QString newDatasetName = datasetRenames[originalDataset];
        QString baseDatasetDir = destinationPath + "/" + newDatasetName;

        if (!unmatchedJPGFiles.isEmpty())
        {
            QString supportDir = baseDatasetDir + "/otherjpg";
            QDir().mkpath(supportDir);
            qDebug() << "\n  Copying" << unmatchedJPGFiles.size() << "unmatched JPG files to:" << supportDir;

            int fileNum = 0;
            for (const QString& jpgPath : unmatchedJPGFiles)
            {
                fileNum++;
                QFileInfo jpgInfo(jpgPath);
                QString targetPath = supportDir + "/" + jpgInfo.fileName();
                qint64 fileSize = jpgInfo.size();
                QElapsedTimer copyTimer;
                copyTimer.start();
                if (QFile::exists(targetPath)) QFile::remove(targetPath);
                if (QFile::copy(jpgPath, targetPath))
                {
                    qint64 copyTime = copyTimer.elapsed();
                    double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                    totalFilesCopied++;
                    totalBytesCopied += fileSize;
                    qDebug() << "    [" << fileNum << "/" << unmatchedJPGFiles.size() << "]"
                             << jpgInfo.fileName() << "-" << (fileSize / 1024.0 / 1024.0) << "MB"
                             << "in" << copyTime << "ms" << "(" << speedMBps << "MB/s)";
                }
                else
                    qDebug() << "    FAILED:" << jpgInfo.fileName();
            }
        }

        if (!unmatchedPNGFiles.isEmpty())
        {
            QString supportDir = baseDatasetDir + "/otherpng";
            QDir().mkpath(supportDir);
            qDebug() << "\n  Copying" << unmatchedPNGFiles.size() << "unmatched PNG files to:" << supportDir;

            int fileNum = 0;
            for (const QString& pngPath : unmatchedPNGFiles)
            {
                fileNum++;
                QFileInfo pngInfo(pngPath);
                QString targetPath = supportDir + "/" + pngInfo.fileName();
                qint64 fileSize = pngInfo.size();
                QElapsedTimer copyTimer;
                copyTimer.start();
                if (QFile::exists(targetPath)) QFile::remove(targetPath);
                if (QFile::copy(pngPath, targetPath))
                {
                    qint64 copyTime = copyTimer.elapsed();
                    double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                    totalFilesCopied++;
                    totalBytesCopied += fileSize;
                    qDebug() << "    [" << fileNum << "/" << unmatchedPNGFiles.size() << "]"
                             << pngInfo.fileName() << "-" << (fileSize / 1024.0 / 1024.0) << "MB"
                             << "in" << copyTime << "ms" << "(" << speedMBps << "MB/s)";
                }
                else
                    qDebug() << "    FAILED:" << pngInfo.fileName();
            }
        }

        if (!unmatchedTIFFFiles.isEmpty())
        {
            QString supportDir = baseDatasetDir + "/othertiff";
            QDir().mkpath(supportDir);
            qDebug() << "\n  Copying" << unmatchedTIFFFiles.size() << "unmatched TIFF files to:" << supportDir;

            int fileNum = 0;
            for (const QString& tiffPath : unmatchedTIFFFiles)
            {
                fileNum++;
                QFileInfo tiffInfo(tiffPath);
                QString targetPath = supportDir + "/" + tiffInfo.fileName();
                qint64 fileSize = tiffInfo.size();
                QElapsedTimer copyTimer;
                copyTimer.start();
                if (QFile::exists(targetPath)) QFile::remove(targetPath);
                if (QFile::copy(tiffPath, targetPath))
                {
                    qint64 copyTime = copyTimer.elapsed();
                    double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                    totalFilesCopied++;
                    totalBytesCopied += fileSize;
                    qDebug() << "    [" << fileNum << "/" << unmatchedTIFFFiles.size() << "]"
                             << tiffInfo.fileName() << "-" << (fileSize / 1024.0 / 1024.0) << "MB"
                             << "in" << copyTime << "ms" << "(" << speedMBps << "MB/s)";
                }
                else
                    qDebug() << "    FAILED:" << tiffInfo.fileName();
            }
        }

        if (gainFileExists)
        {
            QString targetGainFile = baseDatasetDir + "/gain.gain";
            QFileInfo gainInfo(gainFile);
            qint64 fileSize = gainInfo.size();
            QElapsedTimer copyTimer;
            copyTimer.start();
            if (QFile::exists(targetGainFile)) QFile::remove(targetGainFile);
            if (QFile::copy(gainFile, targetGainFile))
            {
                qint64 copyTime = copyTimer.elapsed();
                double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                totalFilesCopied++;
                totalBytesCopied += fileSize;
                qDebug() << "    gain.gain -" << (fileSize / 1024.0 / 1024.0) << "MB"
                         << "in" << copyTime << "ms" << "(" << speedMBps << "MB/s)";
            }
        }

        if (glaciosFileExists)
        {
            QString targetGlaciosFile = baseDatasetDir + "/Glacios_Data_Collection_Parameters.txt";
            QFileInfo glaciosInfo(glaciosFile);
            qint64 fileSize = glaciosInfo.size();
            QElapsedTimer copyTimer;
            copyTimer.start();
            if (QFile::exists(targetGlaciosFile)) QFile::remove(targetGlaciosFile);
            if (QFile::copy(glaciosFile, targetGlaciosFile))
            {
                qint64 copyTime = copyTimer.elapsed();
                double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                totalFilesCopied++;
                totalBytesCopied += fileSize;
                qDebug() << "    Glacios_Data_Collection_Parameters.txt -" << (fileSize / 1024.0 / 1024.0) << "MB"
                         << "in" << copyTime << "ms" << "(" << speedMBps << "MB/s)";
            }
        }

        qDebug() << "\n  Saving graphs to:" << baseDatasetDir;
        QElapsedTimer graphTimer;

        for (int i = 0; i < ui->tabWidgetGraph->count(); ++i)
        {
            if (ui->tabWidgetGraph->tabText(i) == originalDataset)
            {
                QWidget *graphTab = ui->tabWidgetGraph->widget(i);
                QVBoxLayout *outerLayout = qobject_cast<QVBoxLayout*>(graphTab->layout());
                QScrollArea *scrollArea = qobject_cast<QScrollArea*>(outerLayout->itemAt(0)->widget());
                QWidget *scrollContent = scrollArea->widget();
                QVBoxLayout *scrollLayout = qobject_cast<QVBoxLayout*>(scrollContent->layout());
                QWidget *plotsRowWidget = qobject_cast<QWidget*>(scrollLayout->itemAt(0)->widget());
                QHBoxLayout *plotsRowLayout = qobject_cast<QHBoxLayout*>(plotsRowWidget->layout());

                QWidget *totalDoseWidget = qobject_cast<QWidget*>(plotsRowLayout->itemAt(0)->widget());
                QVBoxLayout *totalDoseLayout = qobject_cast<QVBoxLayout*>(totalDoseWidget->layout());
                QCustomPlot *plotTotalDose = qobject_cast<QCustomPlot*>(totalDoseLayout->itemAt(0)->widget());

                QWidget *beamShiftsWidget = qobject_cast<QWidget*>(plotsRowLayout->itemAt(1)->widget());
                QVBoxLayout *beamShiftsLayout = qobject_cast<QVBoxLayout*>(beamShiftsWidget->layout());
                QCustomPlot *plotBeamShifts = qobject_cast<QCustomPlot*>(beamShiftsLayout->itemAt(0)->widget());

                QString dosePlotPath = baseDatasetDir + "/total_dose_plot.png";
                graphTimer.restart();
                plotTotalDose->savePng(dosePlotPath, 1200, 800, 1.0, -1);
                qDebug() << "    total_dose_plot.png saved in" << graphTimer.elapsed() << "ms";

                QString beamPlotPath = baseDatasetDir + "/beam_shifts_plot.png";
                graphTimer.restart();
                plotBeamShifts->savePng(beamPlotPath, 1200, 800, 1.0, -1);
                qDebug() << "    beam_shifts_plot.png saved in" << graphTimer.elapsed() << "ms";


                QGroupBox *sec2Box = graphTab->findChild<QGroupBox*>();
                QGridLayout *sec2Layout = sec2Box ? qobject_cast<QGridLayout*>(sec2Box->layout()) : nullptr;
                QComboBox *microscopeCombo = sec2Layout ? qobject_cast<QComboBox*>(sec2Layout->itemAtPosition(0, 1)->widget()) : nullptr;
                bool isTundra = microscopeCombo && microscopeCombo->currentText().contains("Tundra", Qt::CaseInsensitive);
                if (isTundra)
                {
                    saveDatasetReport(datasetRenames[originalDataset], baseDatasetDir, graphTab, datasetTables[originalDataset]);
                    qDebug() << "    Reports saved (Tundra microscope)";
                }
                else
                    qDebug() << "    Reports skipped (not Tundra - microscope:" << (microscopeCombo ? microscopeCombo->currentText() : "unknown") << ")";

                break;
            }
        }

        qDebug() << "\n  Saving table CSV to:" << baseDatasetDir;
        QElapsedTimer csvTimer;

        for (int i = 0; i < ui->tabWidgetFiles->count(); ++i)
        {
            if (ui->tabWidgetFiles->tabText(i) == originalDataset)
            {
                QWidget *tableTab = ui->tabWidgetFiles->widget(i);
                QVBoxLayout *tableLayout = qobject_cast<QVBoxLayout*>(tableTab->layout());

                if (tableLayout && tableLayout->count() >= 1)
                {
                    QTableWidget *tableWidget = qobject_cast<QTableWidget*>(tableLayout->itemAt(0)->widget());
                    if (tableWidget)
                    {
                        QString csvPath = baseDatasetDir + "/dataset_info.csv";
                        QFile csvFile(csvPath);
                        csvTimer.restart();
                        if (csvFile.open(QIODevice::WriteOnly | QIODevice::Text))
                        {
                            QTextStream stream(&csvFile);
                            QStringList headers;
                            for (int col = 0; col < tableWidget->columnCount(); ++col)
                                headers << tableWidget->horizontalHeaderItem(col)->text();
                            stream << headers.join(",") << "\n";
                            for (int row = 0; row < tableWidget->rowCount(); ++row)
                            {
                                QStringList rowData;
                                for (int col = 0; col < tableWidget->columnCount(); ++col)
                                {
                                    QTableWidgetItem *item = tableWidget->item(row, col);
                                    QString cellData = item ? item->text() : "";
                                    if (cellData.contains(",") || cellData.contains("\""))
                                        cellData = "\"" + cellData.replace("\"", "\"\"") + "\"";
                                    rowData << cellData;
                                }
                                stream << rowData.join(",") << "\n";
                            }
                            csvFile.close();
                            QFileInfo csvInfo(csvPath);
                            qDebug() << "    dataset_info.csv (" << tableWidget->rowCount() << "rows,"
                                     << (csvInfo.size() / 1024.0) << "KB) saved in" << csvTimer.elapsed() << "ms";
                        }
                    }
                }
                break;
            }
        }
    }

    qint64 step7Time = stepTimer.elapsed();
    double avgSpeedMBps = (totalBytesCopied / 1024.0 / 1024.0) / (step7Time / 1000.0);
    qDebug() << "\nStep 7: COMPLETED in" << step7Time << "ms";
    qDebug() << "  Total files copied:" << totalFilesCopied;
    qDebug() << "  Total data copied:" << (totalBytesCopied / 1024.0 / 1024.0) << "MB";
    qDebug() << "  Average speed:" << avgSpeedMBps << "MB/s";

    stepTimer.restart();
    qDebug() << "Step 8: Copy raw files and XML files";
    qDebug() << "Step 8: datasets to process:" << selectedDatasets.size();

    auto getRelativeBelowImagesDisc1 = [](const QString& fullPath) -> QString {
        QStringList parts = fullPath.split("/");
        int idx = -1;
        for (int i = 0; i < parts.size(); ++i) {
            if (parts[i].startsWith("Images-Disc", Qt::CaseInsensitive)) {
                idx = i;
                break;
            }
        }
        if (idx < 0) return QFileInfo(fullPath).fileName();
        return parts.mid(idx + 1).join("/");
    };

    for (const QString& originalDataset : selectedDatasets)
    {
        qDebug() << "Step 8: processing dataset:" << originalDataset;
        qDebug() << "Step 8: peaks found:" << rowNumbersPerPeak[originalDataset].size();
        for (auto it = rowNumbersPerPeak[originalDataset].constBegin(); it != rowNumbersPerPeak[originalDataset].constEnd(); ++it)
            qDebug() << "  Peak" << it.key() << ":" << it.value().size() << "rows";

        QString newDatasetName = datasetRenames[originalDataset];
        QString baseDatasetDir = destinationPath + "/" + newDatasetName;
        QString rawType = datasetRawType[originalDataset].toLower();
        QTableWidget *datasetTable = datasetTables[originalDataset];

        qDebug() << "Step 8: datasetTable valid:" << (datasetTable != nullptr);
        if (datasetTable)
            qDebug() << "Step 8: datasetTable rows:" << datasetTable->rowCount();

        bool multiplePeaks = (rowNumbersPerPeak[originalDataset].size() > 1);

        for (auto it = rowNumbersPerPeak[originalDataset].constBegin(); it != rowNumbersPerPeak[originalDataset].constEnd(); ++it)
        {
            int peak = it.key();
            const QList<int>& rows = it.value();

            QString peakDir  = multiplePeaks ? baseDatasetDir + QString("/peak%1").arg(peak) : baseDatasetDir;
            QString xmlDir   = peakDir + "/data/xml";
            QString rawDir   = peakDir + "/data/raw/" + rawType;
            QString movieDir = peakDir + "/data/movies";

            qDebug() << "\n  Peak" << peak << ":" << rows.size() << "rows to copy";
            qDebug() << "  xmlDir:" << xmlDir;
            qDebug() << "  rawDir:" << rawDir;

            for (int i = 0; i < rows.size(); ++i)
            {
                int row = rows[i];

                QString xmlPath   = datasetTable->item(row, 9)->text();
                QString directory = datasetTable->item(row, 1)->text();
                QString filename  = datasetTable->item(row, 2)->text();
                QString extension = (rawType == "eer") ? ".eer" : (rawType == "tiff" ? ".tiff" : ".mrc");
                QString rawPath   = directory + "/" + filename + extension;
                int groupNumber   = datasetTable->item(row, 23)->text().toInt();

                QString xmlRelative = getRelativeBelowImagesDisc1(xmlPath);
                QString targetXml   = xmlDir + "/" + xmlRelative;
                QString rawRelative = getRelativeBelowImagesDisc1(rawPath);
                QString targetRaw   = rawDir + "/" + rawRelative;

                qDebug() << "  [" << (i+1) << "/" << rows.size() << "]";
                qDebug() << "    XML SRC :" << xmlPath;
                qDebug() << "    XML DST :" << targetXml;
                qDebug() << "    RAW SRC :" << rawPath;
                qDebug() << "    RAW DST :" << targetRaw;
                qDebug() << "    XML exists:" << QFile::exists(xmlPath);
                qDebug() << "    RAW exists:" << QFile::exists(rawPath);

                QFileInfo xmlInfo(xmlPath);
                QFileInfo rawInfo(rawPath);
                QElapsedTimer copyTimer;
                copyTimer.start();

                QDir().mkpath(QFileInfo(targetXml).absolutePath());
                if (QFile::exists(targetXml)) QFile::remove(targetXml);
                if (QFile::copy(xmlPath, targetXml))
                {
                    qint64 copyTime = copyTimer.elapsed();
                    qint64 fileSize = xmlInfo.size();
                    double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                    qDebug() << "    XML COPIED:" << xmlRelative
                             << (fileSize / 1024.0) << "KB" << copyTime << "ms" << speedMBps << "MB/s";
                    currentOperation++;
                    ui->progressBarMovies->setValue(currentOperation);
                }
                else
                    qDebug() << "    FAILED XML:" << xmlRelative;

                copyTimer.restart();
                QDir().mkpath(QFileInfo(targetRaw).absolutePath());
                if (QFile::exists(targetRaw)) QFile::remove(targetRaw);
                if (QFile::copy(rawPath, targetRaw))
                {
                    qint64 copyTime = copyTimer.elapsed();
                    qint64 fileSize = rawInfo.size();
                    double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                    qDebug() << "    RAW COPIED:" << rawRelative
                             << (fileSize / 1024.0 / 1024.0) << "MB" << copyTime << "ms" << speedMBps << "MB/s";
                    currentOperation++;
                    ui->progressBarMovies->setValue(currentOperation);

                    QString movieLinkName = QString("movie_%1_Group%2.%3").arg(i).arg(groupNumber).arg(rawType);
                    QString movieLinkPath = movieDir + "/" + movieLinkName;
                    if (QFile::exists(movieLinkPath)) QFile::remove(movieLinkPath);

                    QString relativeTarget = "../raw/" + rawType + "/" + getRelativeBelowImagesDisc1(rawPath);
                    if (QFile::link(relativeTarget, movieLinkPath))
                    {
                        qDebug() << "    LINK:" << movieLinkName;
                        currentOperation++;
                        ui->progressBarMovies->setValue(currentOperation);
                    }
                    else
                        qDebug() << "    FAILED LINK:" << movieLinkName;
                }
                else
                    qDebug() << "    FAILED RAW:" << rawRelative;
            }
        }
    }

    qDebug() << "Step 8: COMPLETED in" << stepTimer.elapsed() << "ms";

    QApplication::restoreOverrideCursor();
}

void MainWindow::on_pushButtonMoviesDirectory_clicked()
{
    QFileDialog dialog(this);
    dialog.setOptions(QFileDialog::HideNameFilterDetails | QFileDialog::DontUseNativeDialog);
    dialog.setDirectory("/home/data/OffloadData/");
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setLabelText(QFileDialog::FileName,"Folder");
    QStringList ListOfFiles;
    if (dialog.exec())
    {
        ListOfFiles=dialog.selectedFiles();
    }
    else
    {
        return;
    }

    startdir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    startdir.setSorting(QDir::Name | QDir::Reversed);
    startdir.setPath(ListOfFiles[0]);
    qDebug() << startdir.path();
    ui->labelSelectedDirectoryMovie->setText(startdir.path());

    QElapsedTimer timer;
    timer.start();
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QString path = QDir::cleanPath(ui->labelSelectedDirectoryMovie->text());
    currentdir = QDir(path);
    QStringList MovieFilter;
    MovieFilter << "FoilHole*Data*.xml" << "*.eer" << "*.tiff" << "*.mrc" << "*.jpg" << "*.png";

    int totalFiles = 0;
    QDirIterator counter(path, MovieFilter, QDir::AllEntries | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (counter.hasNext()) {
        counter.next();
        if (counter.fileInfo().isFile()) totalFiles++;
    }

    ui->progressBarMovies->setMaximum(totalFiles);
    ui->progressBarMovies->setMinimum(0);
    ui->progressBarMovies->setValue(0);

    allFiles.clear();

    int xmlCount = 0, eerCount = 0, tiffCount = 0, mrcCount = 0, jpgCount = 0, pngCount = 0, largeTiffCount = 0, smallTiffCount = 0;
    qint64 minFileSize = 100 * 1024 * 1024;

    QDirIterator itAllFiles(path, MovieFilter, QDir::AllEntries | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    int processedFiles = 0;
    QMap<QString, QStringList> imageFiles;

    while (itAllFiles.hasNext())
    {
        itAllFiles.next();
        if (itAllFiles.fileInfo().isFile())
        {
            processedFiles++;
            ui->progressBarMovies->setValue(processedFiles);

            QString fullPath = itAllFiles.filePath();
            QString extension = itAllFiles.fileInfo().suffix().toLower();

            if (extension == "mrc" && itAllFiles.fileInfo().size() < minFileSize)
                continue;

            if (extension == "xml")
            {
                QStringList pathParts = fullPath.split("/");
                int imagesDiscIndex = pathParts.indexOf("Images-Disc1");
                if (imagesDiscIndex > 0)
                {
                    xmlCount++;
                    QString datasetName = pathParts[imagesDiscIndex - 1];
                    datasetName = datasetName.trimmed();
                    datasetName = datasetName.replace(" ", "_");
                    if (datasetName.endsWith("_"))
                        datasetName = datasetName.left(datasetName.length() - 1);

                    FileInfo info;
                    info.datasetName = datasetName;
                    info.fileName = itAllFiles.fileName();
                    info.extension = extension;
                    info.baseName = QFileInfo(itAllFiles.fileName()).baseName();
                    info.xmlPath = fullPath;
                    allFiles.append(info);
                }
            }
            else if (extension == "eer" || extension == "tiff" || extension == "mrc" ||
                     extension == "jpg" || extension == "png")
            {
                QString baseName = QFileInfo(itAllFiles.fileName()).baseName();
                imageFiles[baseName].append(fullPath);

                if (extension == "eer") eerCount++;
                else if (extension == "tiff") {
                    tiffCount++;
                    if (itAllFiles.fileInfo().size() >= minFileSize) largeTiffCount++;
                    else smallTiffCount++;
                }
                else if (extension == "mrc") mrcCount++;
                else if (extension == "jpg") jpgCount++;
                else if (extension == "png") pngCount++;
            }
        }
    }

    ui->labelXMLcount->setText(QString("%0 XML files found").arg(xmlCount));
    ui->labelEERcount->setText(QString("%0 EER files found").arg(eerCount));
    ui->labelTIFFcount->setText(QString("%0 TIFF files found (%1 large, %2 small)").arg(tiffCount).arg(largeTiffCount).arg(smallTiffCount));
    ui->labelMRCcount->setText(QString("%0 MRC files found").arg(mrcCount));
    ui->labelJPGcount->setText(QString("%0 JPG files found").arg(jpgCount));
    ui->labelPNGcount->setText(QString("%0 PNG files found").arg(pngCount));

    QSet<QString> matchedImagePaths;
    for (auto& file : allFiles)
    {
        if (file.extension == "xml")
        {
            for (auto it = imageFiles.begin(); it != imageFiles.end(); ++it) {
                if (it.key().contains(file.baseName) || file.baseName.contains(it.key())) {
                    for (const QString& imgPath : it.value()) {
                        if (!file.imagePaths.contains(imgPath))
                            file.imagePaths.append(imgPath);
                        matchedImagePaths.insert(imgPath);
                    }
                }
            }
        }
    }

    unmatchedJPGFiles.clear();
    unmatchedPNGFiles.clear();
    unmatchedTIFFFiles.clear();

    for (auto it = imageFiles.begin(); it != imageFiles.end(); ++it) {
        for (const QString& imgPath : it.value()) {
            if (!matchedImagePaths.contains(imgPath)) {
                QString ext = QFileInfo(imgPath).suffix().toLower();
                if (ext == "jpg") unmatchedJPGFiles.append(imgPath);
                else if (ext == "png") unmatchedPNGFiles.append(imgPath);
                else if (ext == "tiff") unmatchedTIFFFiles.append(imgPath);
            }
        }
    }

    qDebug() << "Unmatched JPG:" << unmatchedJPGFiles.size();
    qDebug() << "Unmatched PNG:" << unmatchedPNGFiles.size();
    qDebug() << "Unmatched TIFF:" << unmatchedTIFFFiles.size();

    QMap<QString, DatasetInfo> datasetInfoMap;
    for (const auto& xmlFile : allFiles)
    {
        QString dataset = xmlFile.datasetName;
        for (const QString& imgPath : xmlFile.imagePaths) {
            QString ext = QFileInfo(imgPath).suffix().toLower();
            if (ext == "eer") datasetInfoMap[dataset].eerCount++;
            else if (ext == "tiff") datasetInfoMap[dataset].tiffCount++;
            else if (ext == "mrc") datasetInfoMap[dataset].mrcCount++;
            else if (ext == "jpg") datasetInfoMap[dataset].jpgCount++;
            else if (ext == "png") datasetInfoMap[dataset].pngCount++;
        }
    }

    QMap<QString, int> xmlCounts;
    QMap<QString, int> totalMatchesByDataset;
    int totalMatches = 0;

    for (const auto& file : allFiles)
    {
        if (file.extension == "xml")
        {
            xmlCounts[file.datasetName]++;
            if (!file.imagePaths.isEmpty())
            {
                totalMatches++;
                totalMatchesByDataset[file.datasetName]++;
            }
        }
    }

    qDebug() << "TOTAL FILE COUNTS - XML:" << xmlCount << "EER:" << eerCount << "TIFF:" << tiffCount << "MRC:" << mrcCount << "JPG:" << jpgCount << "PNG:" << pngCount;
    qDebug() << "Total matches found:" << totalMatches;

    for (auto it = totalMatchesByDataset.begin(); it != totalMatchesByDataset.end(); ++it)
        qDebug() << "Dataset:" << it.key() << "XML files:" << xmlCounts[it.key()] << "Matched files:" << it.value();

    ui->tableWidgetDatasets->setColumnCount(5);
    QStringList headers;
    headers << "Select" << "Dataset Name" << "XML Count" << "Matched Files" << "Extension";
    ui->tableWidgetDatasets->setHorizontalHeaderLabels(headers);
    ui->tableWidgetDatasets->setRowCount(0);

    int row = 0;
    for (auto it = xmlCounts.begin(); it != xmlCounts.end(); ++it)
    {
        ui->tableWidgetDatasets->insertRow(row);

        QCheckBox *checkbox = new QCheckBox();
        checkbox->setChecked(it.value() > 300);
        ui->tableWidgetDatasets->setCellWidget(row, 0, checkbox);

        QTableWidgetItem *nameItem = new QTableWidgetItem(it.key());
        nameItem->setFlags(nameItem->flags() ^ Qt::ItemIsEditable);
        ui->tableWidgetDatasets->setItem(row, 1, nameItem);

        QTableWidgetItem *xmlCountItem = new QTableWidgetItem(QString::number(it.value()));
        xmlCountItem->setFlags(xmlCountItem->flags() ^ Qt::ItemIsEditable);
        xmlCountItem->setTextAlignment(Qt::AlignCenter);
        ui->tableWidgetDatasets->setItem(row, 2, xmlCountItem);

        int matchedCount = totalMatchesByDataset.value(it.key(), 0);
        QTableWidgetItem *matchedCountItem = new QTableWidgetItem(QString::number(matchedCount));
        matchedCountItem->setFlags(matchedCountItem->flags() ^ Qt::ItemIsEditable);
        matchedCountItem->setTextAlignment(Qt::AlignCenter);
        ui->tableWidgetDatasets->setItem(row, 3, matchedCountItem);

        QSet<QString> extensionSet;
        for (const auto& file : allFiles) {
            if (file.datasetName == it.key() && !file.imagePaths.isEmpty()) {
                for (const QString& imgPath : file.imagePaths) {
                    QString ext = QFileInfo(imgPath).suffix().toUpper();
                    if (ext == "EER" || ext == "TIFF" || ext == "MRC")
                        extensionSet.insert(ext);
                }
            }
        }

        QString extension = "NONE";
        if (!extensionSet.isEmpty()) {
            QStringList extList = extensionSet.values();
            std::sort(extList.begin(), extList.end());
            extension = extList.join("+");
        }

        QTableWidgetItem *extensionItem = new QTableWidgetItem(extension);
        extensionItem->setFlags(extensionItem->flags() ^ Qt::ItemIsEditable);
        extensionItem->setTextAlignment(Qt::AlignCenter);
        ui->tableWidgetDatasets->setItem(row, 4, extensionItem);

        row++;
    }
    ui->tableWidgetDatasets->resizeColumnsToContents();

    ui->tabWidgetFiles->clear();
    ui->tabWidget->setCurrentIndex(2);
    ui->tabWidgetGraph->clear();

    int totalSelectedXMLFiles = 0;
    int processedXMLFiles = ui->progressBarMovies->value();

    for (int i = 0; i < ui->tableWidgetDatasets->rowCount(); ++i) {
        QCheckBox *checkbox = qobject_cast<QCheckBox*>(ui->tableWidgetDatasets->cellWidget(i, 0));
        if (checkbox && checkbox->isChecked())
            totalSelectedXMLFiles += ui->tableWidgetDatasets->item(i, 2)->text().toInt();
    }
    ui->progressBarMovies->setMaximum(ui->progressBarMovies->maximum() + totalSelectedXMLFiles);

    for (int i = 0; i < ui->tableWidgetDatasets->rowCount(); ++i)
    {
        QCheckBox *checkbox = qobject_cast<QCheckBox*>(ui->tableWidgetDatasets->cellWidget(i, 0));
        if (checkbox && checkbox->isChecked())
        {
            QString datasetName = ui->tableWidgetDatasets->item(i, 1)->text();

            QWidget *tab = new QWidget();
            ui->tabWidgetFiles->addTab(tab, datasetName);
            ui->tabWidgetFiles->setCurrentWidget(tab);

            QTableWidget *tableWidget = new QTableWidget(tab);
            tableWidget->setColumnCount(26);
            tableWidget->setSortingEnabled(true);
            QStringList headerList;
            headerList << "0: Item"
                       << "1: Directory"
                       << "2: Filename"
                       << "3: EER"
                       << "4: TIFF"
                       << "5: MRC"
                       << "6: JPG"
                       << "7: PNG"
                       << "8: XML file name"
                       << "9: XML file path"
                       << "10: XML file size"
                       << "11: XML last modified time"
                       << "12: XML acquisition time"
                       << "13: Estimated movie acquisition rate"
                       << "14: Voltage"
                       << "15: Beamshift X"
                       << "16: Beamshift Y"
                       << "17: Beamtilt X"
                       << "18: BeamTilt Y"
                       << "19: Exposure time"
                       << "20: Pixel Size"
                       << "21: Dose per Pixel"
                       << "22: Total Dose"
                       << "23: Group"
                       << "24: peak"
                       << "25: Keep";
            tableWidget->setHorizontalHeaderLabels(headerList);

            QVBoxLayout *layout = new QVBoxLayout(tab);
            layout->addWidget(tableWidget);

            //    TAB GRAPH: outer layout with scroll area                   
            QWidget *tabGraph = new QWidget();
            ui->tabWidgetGraph->addTab(tabGraph, datasetName);
            ui->tabWidgetGraph->setCurrentWidget(tabGraph);

            QVBoxLayout *tabGraphOuterLayout = new QVBoxLayout(tabGraph);
            tabGraphOuterLayout->setContentsMargins(0, 0, 0, 0);

            QScrollArea *scrollArea = new QScrollArea(tabGraph);
            scrollArea->setWidgetResizable(true);
            scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            tabGraphOuterLayout->addWidget(scrollArea);

            QWidget *scrollContent = new QWidget();
            scrollArea->setWidget(scrollContent);
            QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
            scrollLayout->setContentsMargins(4, 4, 4, 4);
            scrollLayout->setSpacing(6);

            //    PLOTS ROW                                                  
            QCustomPlot *plotTotalDose = new QCustomPlot(scrollContent);
            plotTotalDose->setMinimumHeight(300);
            plotTotalDose->plotLayout()->insertRow(0);
            QCPTextElement *titleTotalDose = new QCPTextElement(plotTotalDose, "Total Dose");
            titleTotalDose->setFont(QFont("sans", 16, QFont::Bold));
            plotTotalDose->plotLayout()->addElement(0, 0, titleTotalDose);
            plotTotalDose->xAxis->setLabel("Movie number");
            plotTotalDose->yAxis->setLabel("Total Dose (e/A2)");

            QCustomPlot *plotBeamShifts = new QCustomPlot(scrollContent);
            plotBeamShifts->setMinimumHeight(300);
            QCPCurve *beamCurve = new QCPCurve(plotBeamShifts->xAxis, plotBeamShifts->yAxis);
            beamCurve->setObjectName("beamCurve");
            beamCurve->setScatterStyle(QCPScatterStyle::ssCross);
            beamCurve->setLineStyle(QCPCurve::lsNone);
            plotBeamShifts->plotLayout()->insertRow(0);
            QCPTextElement *titleBeamShifts = new QCPTextElement(plotBeamShifts, "Beam Shifts");
            titleBeamShifts->setFont(QFont("sans", 16, QFont::Bold));
            plotBeamShifts->plotLayout()->addElement(0, 0, titleBeamShifts);
            plotBeamShifts->xAxis->setLabel("Beam shift X (A.U.)");
            plotBeamShifts->yAxis->setLabel("Beam shift Y");
            plotBeamShifts->xAxis->setRange(-0.5, 0.5);
            plotBeamShifts->yAxis->setRange(-0.5, 0.5);
            plotBeamShifts->replot();

            QLabel *textLabelTotalDose = new QLabel(scrollContent);
            textLabelTotalDose->setFont(QFont(font().family(), 10));
            textLabelTotalDose->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            textLabelTotalDose->setWordWrap(true);
            textLabelTotalDose->setFrameStyle(QFrame::Box | QFrame::Plain);
            textLabelTotalDose->setMargin(5);
            textLabelTotalDose->setMaximumHeight(60);

            QVBoxLayout *totalDoseLayout = new QVBoxLayout();
            totalDoseLayout->addWidget(plotTotalDose);
            totalDoseLayout->addWidget(textLabelTotalDose);

            QWidget *totalDoseWidget = new QWidget(scrollContent);
            totalDoseWidget->setLayout(totalDoseLayout);

            // Beam shifts widget: plot directly at top, no label/lineedit/button
            QWidget *beamShiftsWidget = new QWidget(scrollContent);
            QVBoxLayout *beamShiftsLayout = new QVBoxLayout(beamShiftsWidget);
            beamShiftsLayout->setContentsMargins(0, 0, 0, 0);
            beamShiftsLayout->addWidget(plotBeamShifts);

            QHBoxLayout *plotsRowLayout = new QHBoxLayout();
            plotsRowLayout->addWidget(totalDoseWidget);
            plotsRowLayout->addWidget(beamShiftsWidget);
            plotsRowLayout->setStretch(0, 1);
            plotsRowLayout->setStretch(1, 1);

            QWidget *plotsRowWidget = new QWidget(scrollContent);
            plotsRowWidget->setLayout(plotsRowLayout);
            scrollLayout->addWidget(plotsRowWidget);

            //    SECTION 1: Sample & Grid                                   
            QGroupBox *sec1Box = new QGroupBox("1. Sample & Grid Information", scrollContent);
            sec1Box->setFont(QFont("sans", 9, QFont::Bold));
            QGridLayout *sec1Layout = new QGridLayout(sec1Box);
            sec1Layout->setColumnStretch(1, 1);
            sec1Layout->setColumnStretch(3, 1);
            sec1Layout->setHorizontalSpacing(8);
            sec1Layout->setVerticalSpacing(4);

            auto addField = [&](QGridLayout *gl, int r, int c, const QString &label, QWidget *widget) {
                QLabel *lbl = new QLabel(label, scrollContent);
                lbl->setFont(QFont("sans", 8));
                lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                gl->addWidget(lbl, r, c);
                gl->addWidget(widget, r, c + 1);
            };

            QLineEdit *sampleNameEdit = new QLineEdit(scrollContent);
            sampleNameEdit->setPlaceholderText("e.g. Apoferritin");
            addField(sec1Layout, 0, 0, "Sample Name:", sampleNameEdit);

            QComboBox *gridTypeCombo = new QComboBox(scrollContent);
            gridTypeCombo->addItems({"Quantifoil R1.2/1.3 300 mesh Cu", "Quantifoil R2/1 300 mesh Cu",
                                     "Quantifoil R1.2/1.3 200 mesh Au", "UltrAuFoil R1.2/1.3 300 mesh Au",
                                     "C-flat CF-1.2/1.3 400 mesh Cu", "Other"});
            gridTypeCombo->setEditable(true);
            addField(sec1Layout, 0, 2, "Grid Type:", gridTypeCombo);

            QLineEdit *waitTimeEdit = new QLineEdit(scrollContent);
            waitTimeEdit->setPlaceholderText("seconds");
            addField(sec1Layout, 1, 0, "Waiting Time Before Blotting (s):", waitTimeEdit);

            QLineEdit *blotTimeEdit = new QLineEdit(scrollContent);
            blotTimeEdit->setPlaceholderText("seconds");
            addField(sec1Layout, 1, 2, "Blotting Time (s):", blotTimeEdit);

            QLineEdit *blotForceEdit = new QLineEdit(scrollContent);
            blotForceEdit->setPlaceholderText("e.g. 0");
            addField(sec1Layout, 2, 0, "Blotting Force:", blotForceEdit);

            QLineEdit *humidityEdit = new QLineEdit(scrollContent);
            humidityEdit->setPlaceholderText("%");
            addField(sec1Layout, 2, 2, "Humidity (%):", humidityEdit);

            QLineEdit *tempEdit = new QLineEdit(scrollContent);
            tempEdit->setPlaceholderText("�C");
            addField(sec1Layout, 3, 0, "Temperature (�C):", tempEdit);

            scrollLayout->addWidget(sec1Box);

            //    SECTION 2: Data Collection Parameters                      
            QGroupBox *sec2Box = new QGroupBox("2. Data Collection Parameters", scrollContent);
            sec2Box->setFont(QFont("sans", 9, QFont::Bold));
            QGridLayout *sec2Layout = new QGridLayout(sec2Box);
            sec2Layout->setColumnStretch(1, 1);
            sec2Layout->setColumnStretch(3, 1);
            sec2Layout->setHorizontalSpacing(8);
            sec2Layout->setVerticalSpacing(4);

            QComboBox *microscopeCombo = new QComboBox(scrollContent);
            microscopeCombo->addItems({"Tundra", "Titan Krios G4", "Titan Krios G3i", "Titan Krios G2", "Glacios", "Talos Arctica", "Other"});
            microscopeCombo->setEditable(true);
            addField(sec2Layout, 0, 0, "Microscope:", microscopeCombo);

            QComboBox *cameraCombo = new QComboBox(scrollContent);
            cameraCombo->addItems({"Falcon C", "Falcon 4i", "Falcon 4", "Falcon 3EC", "K3", "K2 Summit", "Gatan Rio", "Other"});
            cameraCombo->setEditable(true);
            addField(sec2Layout, 0, 2, "Camera:", cameraCombo);

            QComboBox *csCombo = new QComboBox(scrollContent);
            csCombo->addItems({"1.9", "2.7"});
            csCombo->setEditable(true);
            addField(sec2Layout, 1, 0, "Cs (mm):", csCombo);

            QComboBox *voltageCombo = new QComboBox(scrollContent);
            voltageCombo->addItems({"100", "200", "300"});
            voltageCombo->setEditable(true);
            addField(sec2Layout, 1, 2, "Voltage (kV):", voltageCombo);

            QComboBox *magCombo = new QComboBox(scrollContent);
            magCombo->setEditable(true);
            magCombo->setPlaceholderText("e.g. 105000");
            addField(sec2Layout, 2, 0, "Magnification:", magCombo);

            QComboBox *c2ApertureCombo = new QComboBox(scrollContent);
            c2ApertureCombo->addItems({"50", "70", "100"});
            c2ApertureCombo->setEditable(true);
            addField(sec2Layout, 2, 2, "C2 Aperture (um):", c2ApertureCombo);

            QComboBox *objApertureCombo = new QComboBox(scrollContent);
            objApertureCombo->addItems({"None", "50", "100", "150"});
            objApertureCombo->setEditable(true);
            addField(sec2Layout, 3, 0, "Objective Aperture (um):", objApertureCombo);

            QComboBox *instrumentPixelSizeCombo = new QComboBox(scrollContent);
            instrumentPixelSizeCombo->setEditable(true);
            instrumentPixelSizeCombo->setStyleSheet("background-color: #f0f0f0;");
            addField(sec2Layout, 3, 2, "Instrument Pixel Size (A/px):", instrumentPixelSizeCombo);

            QComboBox *calibratedPixelSizeCombo = new QComboBox(scrollContent);
            calibratedPixelSizeCombo->setEditable(true);
            addField(sec2Layout, 4, 0, "Calibrated Pixel Size (A/px):", calibratedPixelSizeCombo);

            QLineEdit *doseRequestedEdit = new QLineEdit(scrollContent);
            doseRequestedEdit->setPlaceholderText("e/A2");
            addField(sec2Layout, 4, 2, "Total Dose Requested (e/A2):", doseRequestedEdit);

            QLineEdit *doseRecordedEdit = new QLineEdit(scrollContent);
            doseRecordedEdit->setReadOnly(true);
            doseRecordedEdit->setStyleSheet("background-color: #f0f0f0;");
            addField(sec2Layout, 5, 0, "Total Dose Recorded (e/A2):", doseRecordedEdit);

            QLineEdit *totalMoviesEdit = new QLineEdit(scrollContent);
            totalMoviesEdit->setReadOnly(true);
            totalMoviesEdit->setStyleSheet("background-color: #f0f0f0;");
            totalMoviesEdit->setText(QString::number(tableWidget->rowCount()));
            addField(sec2Layout, 5, 2, "Total Movies Collected:", totalMoviesEdit);

            QLineEdit *moviesKeptEdit = new QLineEdit(scrollContent);
            moviesKeptEdit->setReadOnly(true);
            moviesKeptEdit->setStyleSheet("background-color: #f0f0f0;");
            addField(sec2Layout, 6, 0, "Movies Kept:", moviesKeptEdit);

            QLineEdit *moviesRejectedEdit = new QLineEdit(scrollContent);
            moviesRejectedEdit->setReadOnly(true);
            moviesRejectedEdit->setStyleSheet("background-color: #f0f0f0;");
            addField(sec2Layout, 6, 2, "Movies Rejected:", moviesRejectedEdit);

            QComboBox *acqModeCombo = new QComboBox(scrollContent);
            acqModeCombo->addItems({"Linear", "Counting", "Super-resolution"});
            addField(sec2Layout, 7, 0, "Acquisition Mode:", acqModeCombo);

            QLineEdit *defocusEdit = new QLineEdit(scrollContent);
            defocusEdit->setPlaceholderText("e.g. -1.0 to -3.0 um");
            addField(sec2Layout, 7, 2, "Defocus Range (um):", defocusEdit);

            QLineEdit *epuVersionEdit = new QLineEdit(scrollContent);
            epuVersionEdit->setPlaceholderText("e.g. 3.4.0");
            addField(sec2Layout, 8, 0, "EPU Version:", epuVersionEdit);

            scrollLayout->addWidget(sec2Box);
            scrollLayout->addStretch();

            connect(microscopeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [microscopeCombo, voltageCombo, csCombo]() {
                QString microscope = microscopeCombo->currentText();
                if (microscope.contains("Tundra", Qt::CaseInsensitive)) {
                    voltageCombo->setCurrentText("100");
                    csCombo->setCurrentText("1.9");
                } else if (microscope.contains("Glacios", Qt::CaseInsensitive)) {
                    voltageCombo->setCurrentText("200");
                    csCombo->setCurrentText("2.7");
                } else if (microscope.contains("Krios", Qt::CaseInsensitive)) {
                    voltageCombo->setCurrentText("300");
                    csCombo->setCurrentText("2.7");
                }
            });

            //    XML LOOP                                                   
            int rowCount = 0;

            for (const auto& xmlFile : allFiles)
            {
                if (xmlFile.datasetName == datasetName && xmlFile.extension == "xml" && !xmlFile.imagePaths.isEmpty())
                {
                    processedXMLFiles++;
                    ui->progressBarMovies->setValue(processedXMLFiles);

                    QFileInfo xmlFileInfo(xmlFile.xmlPath);

                    QString primaryPath;
                    for (const QString& p : xmlFile.imagePaths) {
                        if (p.endsWith(".eer", Qt::CaseInsensitive)) { primaryPath = p; break; }
                    }
                    if (primaryPath.isEmpty())
                        for (const QString& p : xmlFile.imagePaths) {
                            if (p.endsWith(".tiff", Qt::CaseInsensitive)) { primaryPath = p; break; }
                        }
                    if (primaryPath.isEmpty())
                        for (const QString& p : xmlFile.imagePaths) {
                            if (p.endsWith(".mrc", Qt::CaseInsensitive)) { primaryPath = p; break; }
                        }
                    if (primaryPath.isEmpty())
                        primaryPath = xmlFile.imagePaths.first();

                    QFileInfo movieFileInfo(primaryPath);

                    auto makeItem = [](const QString& text) {
                        QTableWidgetItem *it = new QTableWidgetItem(text);
                        it->setFlags(it->flags() ^ Qt::ItemIsEditable);
                        it->setTextAlignment(Qt::AlignCenter);
                        return it;
                    };

                    bool hasEER = false, hasTIFF = false, hasMRC = false, hasJPG = false, hasPNG = false;
                    for (const QString& p : xmlFile.imagePaths) {
                        QString ext = QFileInfo(p).suffix().toLower();
                        if (ext == "eer") hasEER = true;
                        else if (ext == "tiff") hasTIFF = true;
                        else if (ext == "mrc") hasMRC = true;
                        else if (ext == "jpg") hasJPG = true;
                        else if (ext == "png") hasPNG = true;
                    }

                    QTableWidgetItem *xmlNameItem = new QTableWidgetItem(xmlFile.fileName);
                    xmlNameItem->setFlags(xmlNameItem->flags() ^ Qt::ItemIsEditable);
                    QTableWidgetItem *xmlPathItem = new QTableWidgetItem(xmlFile.xmlPath);
                    xmlPathItem->setFlags(xmlPathItem->flags() ^ Qt::ItemIsEditable);
                    QTableWidgetItem *xmlSizeItem = new QTableWidgetItem(QLocale().formattedDataSize(xmlFileInfo.size()) + ".");
                    xmlSizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    xmlSizeItem->setFlags(xmlSizeItem->flags() ^ Qt::ItemIsEditable);
                    QTableWidgetItem *xmlModifiedItem = new QTableWidgetItem(QDateTime(xmlFileInfo.lastModified()).toString());
                    xmlModifiedItem->setFlags(xmlModifiedItem->flags() ^ Qt::ItemIsEditable);

                    XMLData xmlData = parseXMLFile(xmlFile.xmlPath);

                    double totalDoseValue = 0.0;
                    QString totalDoseStr = "";
                    if (!xmlData.pixelSize.isEmpty() && !xmlData.dosePerPixel.isEmpty()) {
                        double px = xmlData.pixelSize.toDouble();
                        totalDoseValue = xmlData.dosePerPixel.toDouble() / (px * px);
                        totalDoseStr = QString("%0").arg(totalDoseValue);
                    }

                    QTableWidgetItem *totalDoseItem = new QTableWidgetItem(totalDoseStr);
                    totalDoseItem->setFlags(totalDoseItem->flags() ^ Qt::ItemIsEditable);

                    QCPCurve *bc = qobject_cast<QCPCurve*>(plotBeamShifts->findChild<QCPCurve*>("beamCurve"));
                    if (bc) bc->addData(rowCount, xmlData.beamshiftX.toDouble(), xmlData.beamshiftY.toDouble());
                    bc->rescaleAxes(true);

                    tableWidget->insertRow(rowCount);
                    tableWidget->setItem(rowCount,  0, new QTableWidgetItem(QString("%0").arg(rowCount + 1)));
                    tableWidget->setItem(rowCount,  1, new QTableWidgetItem(movieFileInfo.absolutePath()));
                    tableWidget->setItem(rowCount,  2, new QTableWidgetItem(movieFileInfo.baseName()));
                    tableWidget->setItem(rowCount,  3, makeItem(hasEER  ? "Yes" : "No"));
                    tableWidget->setItem(rowCount,  4, makeItem(hasTIFF ? "Yes" : "No"));
                    tableWidget->setItem(rowCount,  5, makeItem(hasMRC  ? "Yes" : "No"));
                    tableWidget->setItem(rowCount,  6, makeItem(hasJPG  ? "Yes" : "No"));
                    tableWidget->setItem(rowCount,  7, makeItem(hasPNG  ? "Yes" : "No"));
                    tableWidget->setItem(rowCount,  8, xmlNameItem);
                    tableWidget->setItem(rowCount,  9, xmlPathItem);
                    tableWidget->setItem(rowCount, 10, xmlSizeItem);
                    tableWidget->setItem(rowCount, 11, xmlModifiedItem);
                    tableWidget->setItem(rowCount, 12, new QTableWidgetItem(xmlData.acquisitionTime));
                    tableWidget->setItem(rowCount, 13, new QTableWidgetItem(xmlData.movieAcquisitionRate));
                    tableWidget->setItem(rowCount, 14, new QTableWidgetItem(xmlData.voltage));
                    tableWidget->setItem(rowCount, 15, new QTableWidgetItem(xmlData.beamshiftX));
                    tableWidget->setItem(rowCount, 16, new QTableWidgetItem(xmlData.beamshiftY));
                    tableWidget->setItem(rowCount, 17, new QTableWidgetItem(xmlData.beamtiltX));
                    tableWidget->setItem(rowCount, 18, new QTableWidgetItem(xmlData.beamtiltY));
                    tableWidget->setItem(rowCount, 19, new QTableWidgetItem(xmlData.exposureTime));
                    tableWidget->setItem(rowCount, 20, new QTableWidgetItem(xmlData.pixelSize));
                    tableWidget->setItem(rowCount, 21, new QTableWidgetItem(xmlData.dosePerPixel));
                    tableWidget->setItem(rowCount, 22, totalDoseItem);
                    rowCount++;
                }
            }

            DoseAnalysisResults doseResults = analyseAndFilterDose(tableWidget, plotTotalDose, textLabelTotalDose, 1.5);

            doseRecordedEdit->setText(QString::number(doseResults.keptMeanDose, 'f', 2) + " e/A2");
            moviesKeptEdit->setText(QString::number(doseResults.keptCount));
            moviesRejectedEdit->setText(QString::number(doseResults.rejectedCount));
            totalMoviesEdit->setText(QString::number(tableWidget->rowCount()));

            if (tableWidget->rowCount() > 0 && tableWidget->item(0, 20)) {
                QString instrumentPixelSize = tableWidget->item(0, 20)->text();
                instrumentPixelSizeCombo->setCurrentText(instrumentPixelSize);

                if (pixelSizeData.contains(instrumentPixelSize))
                {
                    magCombo->setCurrentText(pixelSizeData[instrumentPixelSize].magnification);
                    calibratedPixelSizeCombo->setCurrentText(pixelSizeData[instrumentPixelSize].calibratedPixelSize);
                }
                else
                {
                    magCombo->setCurrentText("");
                    calibratedPixelSizeCombo->setCurrentText("");
                }
            }

            if (tableWidget->rowCount() > 0 && tableWidget->item(0, 14))
                voltageCombo->setCurrentText(tableWidget->item(0, 14)->text());

            QElapsedTimer clusterTimer;
            clusterTimer.start();
            clusterAndRecolorBeamShifts(plotBeamShifts);
            qDebug() << "Clustering took:" << clusterTimer.elapsed() << "ms";

            for (int tableRow = 0; tableRow < tableWidget->rowCount(); ++tableRow)
            {
                double beamX = tableWidget->item(tableRow, 15)->text().toDouble();
                double beamY = tableWidget->item(tableRow, 16)->text().toDouble();
                int groupNumber = coordinateToCluster.value(QPair<double, double>(beamX, beamY), 0);
                QTableWidgetItem *groupItem = new QTableWidgetItem(QString::number(groupNumber));
                groupItem->setFlags(groupItem->flags() ^ Qt::ItemIsEditable);
                groupItem->setTextAlignment(Qt::AlignCenter);
                tableWidget->setItem(tableRow, 23, groupItem);
            }

            tableWidget->resizeColumnsToContents();
            qDebug() << "Created tab for dataset:" << datasetName << "with" << rowCount << "XML files";
        }
    }

    QString formattedTime = QTime::fromMSecsSinceStartOfDay(timer.elapsed()).toString("hh:mm:ss.zzz");
    qDebug() << formattedTime;
    ui->labelFindingTime->setText("Finding Time: " + formattedTime);
    ui->pushButtonDestinationDirectory->setEnabled(totalMatches > 0);

    QApplication::restoreOverrideCursor();
}

MainWindow::DoseAnalysisResults MainWindow::analyseAndFilterDose(QTableWidget *tableWidget, QCustomPlot *plotTotalDose, QLabel *statsLabel, double minSeparation, double manualMinDose)
{
    // manualMinDose: if >= 0, force-keep any sample with dose >= manualMinDose,
    // regardless of GMM assignment. Pass -1.0 (default) to disable.
    const bool useManualThreshold = (manualMinDose >= 0.0);

    DoseAnalysisResults results;
    results.minSeparation = minSeparation;
    if (!tableWidget || !plotTotalDose) return results;

    qDebug() << "=== START analyseAndFilterDose (minSeparation=" << minSeparation
             << "manualMinDose=" << (useManualThreshold ? QString::number(manualMinDose) : "off") << ") ===";

    // --- 1. Extract dose values ---
    QVector<double> doseValues;
    QVector<int> validRows;

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 22);
        if (item && !item->text().isEmpty()) {
            bool ok;
            double d = item->text().toDouble(&ok);
            if (ok && std::isfinite(d)) {
                doseValues.append(d);
                validRows.append(row);
            }
        }
    }

    int n = doseValues.size();
    qDebug() << "n=" << n;
    if (n < 10) return results;

    double minD = *std::min_element(doseValues.begin(), doseValues.end());
    double maxD = *std::max_element(doseValues.begin(), doseValues.end());

    // --- 2. Fit 1D GMM with BIC, multiple inits ---
    arma::mat data(1, n);
    for (int i = 0; i < n; ++i)
        data(0, i) = doseValues[i];

    int bestK = 1;
    arma::gmm_full bestModel;
    double bestBIC = std::numeric_limits<double>::max();

    for (int k = 1; k <= 10; k++) {
        double bestLogLK = -std::numeric_limits<double>::max();
        arma::gmm_full bestModelK;

        for (int init = 0; init < 20; init++) {
            arma::gmm_full model;
            if (model.learn(data, k, arma::eucl_dist, arma::random_subset, 300, 5, 1e-10, false)) {
                double logL = model.avg_log_p(data) * n;
                if (logL > bestLogLK) {
                    bestLogLK = logL;
                    bestModelK = model;
                }
            }
        }

        if (bestLogLK == -std::numeric_limits<double>::max()) {
            qDebug() << "k=" << k << "all inits failed - stopping";
            break;
        }

        int numParams = k * 3 - 1;
        double bic = -2.0 * bestLogLK + numParams * std::log(n);
        qDebug() << "k=" << k << "logL=" << bestLogLK << "BIC=" << bic;

        if (bic < bestBIC) {
            bestBIC = bic;
            bestK = k;
            bestModel = bestModelK;
        } else if (bic > bestBIC + 10.0) {
            qDebug() << "BIC increasing - early stop at k=" << k;
            break;
        }
    }
    qDebug() << "Raw GMM best k=" << bestK;

    // --- 3. Extract components ---
    struct Component {
        double mean, std, weight;
        int count;
        bool isMain;
    };
    QVector<Component> components;

    QVector<int> compAssignment(n);
    QVector<int> compCounts(bestK, 0);
    for (int i = 0; i < n; ++i) {
        arma::uword idx = bestModel.assign(data.col(i), arma::eucl_dist);
        compAssignment[i] = (int)idx;
        compCounts[(int)idx]++;
    }

    for (int k = 0; k < bestK; ++k) {
        Component c;
        c.mean   = bestModel.means(0, k);
        c.std    = std::sqrt(qMax(bestModel.fcovs.slice(k)(0, 0), 1e-9));
        c.weight = bestModel.hefts(k);
        c.count  = compCounts[k];
        c.isMain = false;
        components.append(c);
    }

    // Seed: highest weight component
    int mainIdx = 0;
    for (int k = 1; k < bestK; ++k)
        if (components[k].weight > components[mainIdx].weight)
            mainIdx = k;

    qDebug() << "Seed component: mean=" << components[mainIdx].mean << "weight=" << components[mainIdx].weight;

    // --- 4. Chain merge ---
    // Merge any component that is within minSeparation BELOW (or anywhere above)
    // any already-merged component. Iterates until stable.
    QVector<bool> mergedIntoMain(bestK, false);
    mergedIntoMain[mainIdx] = true;

    bool changed = true;
    while (changed) {
        changed = false;
        for (int k = 0; k < bestK; ++k) {
            if (mergedIntoMain[k]) continue;
            for (int m = 0; m < bestK; ++m) {
                if (!mergedIntoMain[m]) continue;
                // diff > 0 means k is below m
                double diff = components[m].mean - components[k].mean;
                if (diff < minSeparation) {
                    // k is above m (diff<0), or close enough below → merge
                    qDebug() << "Chain-merging k=" << k
                             << "mean=" << components[k].mean
                             << "relative to m=" << m
                             << "mean=" << components[m].mean
                             << "diff=" << diff;
                    mergedIntoMain[k] = true;
                    changed = true;
                    break;
                }
            }
        }
    }

    // Log remaining outliers
    for (int k = 0; k < bestK; ++k) {
        if (!mergedIntoMain[k])
            qDebug() << "Outlier component k=" << k << "mean=" << components[k].mean;
    }

    // Recompute mainMean as weighted mean of all merged components
    double mergedWeightSum = 0.0, mergedMeanSum = 0.0;
    for (int k = 0; k < bestK; ++k) {
        if (mergedIntoMain[k]) {
            mergedWeightSum += components[k].weight;
            mergedMeanSum   += components[k].weight * components[k].mean;
        }
    }
    double mainMean = (mergedWeightSum > 0) ? mergedMeanSum / mergedWeightSum : components[mainIdx].mean;
    double mainStd  = components[mainIdx].std;
    qDebug() << "Merged main mean (weighted)=" << mainMean;

    for (int k = 0; k < bestK; ++k)
        components[k].isMain = mergedIntoMain[k];

    int numPopulations = 1;
    for (int k = 0; k < bestK; ++k)
        if (!components[k].isMain) numPopulations++;

    qDebug() << "Total populations:" << numPopulations;

    // --- 5. Generate colours: kept=blue, rejected=shades of red ---
    QVector<QColor> rejectedColors(bestK);
    int outlierIdx = 0;
    int numOutliers = numPopulations - 1;
    for (int k = 0; k < bestK; ++k) {
        if (!components[k].isMain) {
            double factor = (numOutliers > 1) ? double(outlierIdx) / double(numOutliers - 1) : 0.0;
            double sat = 1.0 - factor * 0.4;
            double val = 0.9 - factor * 0.4;
            rejectedColors[k] = QColor::fromHsvF(0.0, sat, val);
            outlierIdx++;
        }
    }

    // --- 6. Assign keep/reject ---
    QVector<double> xKept, yKept;
    QVector<QVector<double>> xRej(bestK), yRej(bestK);

    int keptCount = 0, rejectedCount = 0;
    double keptMeanSum = 0.0;

    QVector<double> outlierMeans;
    for (int k = 0; k < bestK; ++k)
        if (!components[k].isMain)
            outlierMeans.append(components[k].mean);
    std::sort(outlierMeans.begin(), outlierMeans.end(), std::greater<double>());

    for (int i = 0; i < n; ++i) {
        int row     = validRows[i];
        double dose = doseValues[i];
        int comp    = compAssignment[i];

        // GMM decision
        bool keep = components[comp].isMain;

        // Manual threshold override: force-keep anything >= manualMinDose
        if (useManualThreshold && dose >= manualMinDose)
            keep = true;

        if (keep) {
            keptCount++;
            keptMeanSum += dose;
            xKept.append(row);
            yKept.append(dose);
        } else {
            rejectedCount++;
            xRej[comp].append(row);
            yRej[comp].append(dose);
        }

        // Column 25: Keep
        QTableWidgetItem *keepItem = new QTableWidgetItem(keep ? "Yes" : "No");
        keepItem->setFlags(keepItem->flags() ^ Qt::ItemIsEditable);
        keepItem->setTextAlignment(Qt::AlignCenter);
        if (!keep)
            keepItem->setBackground(QBrush(QColor(255, 200, 200)));
        tableWidget->setItem(row, 25, keepItem);

        // Column 24: population index (1=main, 2+=outliers sorted by mean descending)
        int peakID = 1;
        if (!keep) {
            int outlierRank = outlierMeans.indexOf(components[comp].mean);
            peakID = (outlierRank >= 0) ? outlierRank + 2 : 2;
        }
        if (tableWidget->item(row, 24))
            tableWidget->item(row, 24)->setText(QString::number(peakID));
        else
            tableWidget->setItem(row, 24, new QTableWidgetItem(QString::number(peakID)));
    }

    double keptMean = keptCount > 0 ? keptMeanSum / keptCount : 0.0;

    // --- 7. Plot ---
    plotTotalDose->clearGraphs();
    plotTotalDose->clearItems();

    int graphIdx = 0;

    // Kept points — blue
    if (!xKept.isEmpty()) {
        plotTotalDose->addGraph();
        plotTotalDose->graph(graphIdx)->setPen(QPen(QColor(0, 100, 200)));
        plotTotalDose->graph(graphIdx)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 4));
        plotTotalDose->graph(graphIdx)->setLineStyle(QCPGraph::lsNone);
        plotTotalDose->graph(graphIdx)->setData(xKept, yKept);
        plotTotalDose->graph(graphIdx)->setName("Kept");
        graphIdx++;
    }

    // Rejected points — shades of red, one graph per outlier component
    for (int k = 0; k < bestK; ++k) {
        if (!xRej[k].isEmpty()) {
            plotTotalDose->addGraph();
            plotTotalDose->graph(graphIdx)->setPen(QPen(rejectedColors[k]));
            plotTotalDose->graph(graphIdx)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 4));
            plotTotalDose->graph(graphIdx)->setLineStyle(QCPGraph::lsNone);
            plotTotalDose->graph(graphIdx)->setData(xRej[k], yRej[k]);
            plotTotalDose->graph(graphIdx)->setName(QString("Rejected (comp %1)").arg(k+1));
            graphIdx++;
        }
    }

    // Threshold line: green dashed if manual, red dashed if GMM-derived
    double threshLineY;
    if (useManualThreshold) {
        threshLineY = manualMinDose;
    } else {
        double lowestMergedMean = std::numeric_limits<double>::max();
        for (int k = 0; k < bestK; ++k)
            if (mergedIntoMain[k] && components[k].mean < lowestMergedMean)
                lowestMergedMean = components[k].mean;
        threshLineY = lowestMergedMean - minSeparation;
    }

    QCPItemLine *line = new QCPItemLine(plotTotalDose);
    line->start->setType(QCPItemPosition::ptPlotCoords);
    line->start->setCoords(0, threshLineY);
    line->end->setType(QCPItemPosition::ptPlotCoords);
    line->end->setCoords(tableWidget->rowCount(), threshLineY);
    line->setPen(QPen(useManualThreshold ? Qt::darkGreen : Qt::red, 1, Qt::DashLine));

    plotTotalDose->xAxis->setRange(-5, tableWidget->rowCount() + 5);
    plotTotalDose->yAxis->setRange(minD - 2.0, maxD + 2.0);

    plotTotalDose->legend->setVisible(true);
    plotTotalDose->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignBottom | Qt::AlignLeft);
    plotTotalDose->legend->setFont(QFont("sans", 8));
    plotTotalDose->replot();

    // --- 8. Stats label ---
    if (statsLabel) {
        QString statsText;
        if (useManualThreshold)
            statsText = QString("MANUAL threshold: dose≥%1 e/Ų | ").arg(manualMinDose, 0, 'f', 2);
        else
            statsText = QString("GMM: %1 raw components, %2 populations (sep≥%3 e/Ų) | ")
                            .arg(bestK).arg(numPopulations).arg(minSeparation, 0, 'f', 1);

        statsText += QString("Kept: %1 (mean=%2 e/Ų) | Rejected: %3")
                         .arg(keptCount)
                         .arg(keptMean, 0, 'f', 2)
                         .arg(rejectedCount);

        for (int k = 0; k < bestK; ++k) {
            statsText += QString(" | Comp%1[%2]: μ=%3 σ=%4 n=%5")
                             .arg(k+1)
                             .arg(components[k].isMain ? "main" : "out")
                             .arg(components[k].mean,  0, 'f', 2)
                             .arg(components[k].std,   0, 'f', 2)
                             .arg(components[k].count);
        }
        statsLabel->setText(statsText);
    }

    // --- 9. Results ---
    results.numPopulations = numPopulations;
    results.mainMean       = mainMean;
    results.mainStd        = mainStd;
    results.keptCount      = keptCount;
    results.rejectedCount  = rejectedCount;
    results.keptMeanDose   = keptMean;

    qDebug() << "Kept=" << keptCount << "Rejected=" << rejectedCount << "Mean(kept)=" << keptMean;
    qDebug() << "=== END analyseAndFilterDose ===";
    return results;
}
