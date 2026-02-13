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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->progressBarMovies->setValue(0);
    ui->pushButtonDestinationDirectory->setEnabled(false);


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

void MainWindow::onRecalculateClustersClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (button) {
        QWidget *parentWidget = button->parentWidget(); // beamShiftsWidget
        QCustomPlot *plotBeamShifts = parentWidget->findChild<QCustomPlot*>();
        if (plotBeamShifts) {
            // Find which tab this plot belongs to
            QWidget *tabGraph = parentWidget->parentWidget(); // the tab widget
            int tabIndex = ui->tabWidgetGraph->indexOf(tabGraph);
            // Get corresponding table widget from files tab
            if (tabIndex >= 0 && tabIndex < ui->tabWidgetFiles->count()) {
                QWidget *tableTab = ui->tabWidgetFiles->widget(tabIndex);
                QTableWidget *tableWidget = tableTab->findChild<QTableWidget*>();
                if (tableWidget) {
                    // Rebuild single graph from table data for re-clustering
                    plotBeamShifts->clearGraphs();
                    plotBeamShifts->addGraph();
                    plotBeamShifts->graph(0)->setScatterStyle(QCPScatterStyle::ssCross);
                    plotBeamShifts->graph(0)->setLineStyle(QCPGraph::lsNone);
                    // Add beam shift coordinates from table to single graph
                    for (int tableRow = 0; tableRow < tableWidget->rowCount(); ++tableRow) {
                        double beamX = tableWidget->item(tableRow, 15)->text().toDouble();  // Changed from 12 to 15
                        double beamY = tableWidget->item(tableRow, 16)->text().toDouble();  // Changed from 13 to 16
                        plotBeamShifts->graph(0)->addData(beamX, beamY);
                    }
                    // Run clustering with new parameters
                    //clusterAndRecolorBeamShifts(plotBeamShifts);

                    qDebug() << "coordinateToCluster size:" << coordinateToCluster.size();

                    // Update Group column with new cluster assignments
                    for (int tableRow = 0; tableRow < tableWidget->rowCount(); ++tableRow) {
                        double beamX = tableWidget->item(tableRow, 15)->text().toDouble();  // Changed from 12 to 15
                        double beamY = tableWidget->item(tableRow, 16)->text().toDouble();  // Changed from 13 to 16
                        int groupNumber = coordinateToCluster.value(QPair<double, double>(beamX, beamY), 0);

                        QTableWidgetItem *groupItem = new QTableWidgetItem(QString::number(groupNumber));
                        groupItem->setFlags(groupItem->flags() ^ Qt::ItemIsEditable);
                        groupItem->setTextAlignment(Qt::AlignCenter);
                        tableWidget->setItem(tableRow, 23, groupItem);  // Changed from 20 to 23
                    }
                }
            }
        }
    }
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
    dialog.setDirectory("/home/data/raw3/2025/");
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
        for (int tabIndex = 0; tabIndex < ui->tabWidgetFiles->count(); ++tabIndex)
        {
            if (ui->tabWidgetFiles->tabText(tabIndex) == originalDataset)
            {
                QWidget *tableTab = ui->tabWidgetFiles->widget(tabIndex);
                QVBoxLayout *tableLayout = qobject_cast<QVBoxLayout*>(tableTab->layout());
                if (tableLayout && tableLayout->count() >= 1)
                {
                    datasetTable = qobject_cast<QTableWidget*>(tableLayout->itemAt(0)->widget());
                }
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

        int eerCount = 0;
        int tiffCount = 0;
        int mrcCount = 0;

        for (int row = 0; row < datasetTable->rowCount(); ++row)
        {
            QString eerExists = datasetTable->item(row, 3)->text();
            QString tiffExists = datasetTable->item(row, 4)->text();
            QString mrcExists = datasetTable->item(row, 5)->text();

            if (eerExists == "Yes") eerCount++;
            if (tiffExists == "Yes") tiffCount++;
            if (mrcExists == "Yes") mrcCount++;

            QTableWidgetItem *keepItem = datasetTable->item(row, 25);
            if (keepItem && keepItem->text() == "Yes")
            {
                int peakNumber = datasetTable->item(row, 24)->text().toInt();
                rowNumbersPerPeak[originalDataset][peakNumber].append(row);
            }
        }

        if (eerCount > 0) {
            datasetRawType[originalDataset] = "EER";
        } else if (tiffCount > 0) {
            datasetRawType[originalDataset] = "TIFF";
        } else if (mrcCount > 0) {
            datasetRawType[originalDataset] = "MRC";
        }

        qDebug() << "    Raw data type:" << datasetRawType[originalDataset];
        qDebug() << "    Dataset" << originalDataset << "organized into" << rowNumbersPerPeak[originalDataset].size() << "peaks";
        for (auto it = rowNumbersPerPeak[originalDataset].constBegin(); it != rowNumbersPerPeak[originalDataset].constEnd(); ++it)
        {
            qDebug() << "      Peak" << it.key() << ":" << it.value().size() << "rows";
        }
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
    {
        qDebug() << "  Glacios file not found";
    }

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
    {
        qDebug() << "  Gain file not found";
    }

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

                if (QFile::exists(targetPath)) {
                    QFile::remove(targetPath);
                }

                if (QFile::copy(jpgPath, targetPath))
                {
                    qint64 copyTime = copyTimer.elapsed();
                    double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                    totalFilesCopied++;
                    totalBytesCopied += fileSize;

                    qDebug() << "    [" << fileNum << "/" << unmatchedJPGFiles.size() << "]"
                             << jpgInfo.fileName()
                             << "-" << (fileSize / 1024.0 / 1024.0) << "MB"
                             << "in" << copyTime << "ms"
                             << "(" << speedMBps << "MB/s)";
                }
                else
                {
                    qDebug() << "    FAILED:" << jpgInfo.fileName();
                }
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

                if (QFile::exists(targetPath)) {
                    QFile::remove(targetPath);
                }

                if (QFile::copy(pngPath, targetPath))
                {
                    qint64 copyTime = copyTimer.elapsed();
                    double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                    totalFilesCopied++;
                    totalBytesCopied += fileSize;

                    qDebug() << "    [" << fileNum << "/" << unmatchedPNGFiles.size() << "]"
                             << pngInfo.fileName()
                             << "-" << (fileSize / 1024.0 / 1024.0) << "MB"
                             << "in" << copyTime << "ms"
                             << "(" << speedMBps << "MB/s)";
                }
                else
                {
                    qDebug() << "    FAILED:" << pngInfo.fileName();
                }
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

                if (QFile::exists(targetPath)) {
                    QFile::remove(targetPath);
                }

                if (QFile::copy(tiffPath, targetPath))
                {
                    qint64 copyTime = copyTimer.elapsed();
                    double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                    totalFilesCopied++;
                    totalBytesCopied += fileSize;

                    qDebug() << "    [" << fileNum << "/" << unmatchedTIFFFiles.size() << "]"
                             << tiffInfo.fileName()
                             << "-" << (fileSize / 1024.0 / 1024.0) << "MB"
                             << "in" << copyTime << "ms"
                             << "(" << speedMBps << "MB/s)";
                }
                else
                {
                    qDebug() << "    FAILED:" << tiffInfo.fileName();
                }
            }
        }

        qDebug() << "\n  Copying support files to:" << baseDatasetDir;

        if (gainFileExists)
        {
            QString targetGainFile = baseDatasetDir + "/gain.gain";
            QFileInfo gainInfo(gainFile);
            qint64 fileSize = gainInfo.size();

            QElapsedTimer copyTimer;
            copyTimer.start();

            if (QFile::exists(targetGainFile)) {
                QFile::remove(targetGainFile);
            }

            if (QFile::copy(gainFile, targetGainFile))
            {
                qint64 copyTime = copyTimer.elapsed();
                double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                totalFilesCopied++;
                totalBytesCopied += fileSize;

                qDebug() << "    gain.gain -" << (fileSize / 1024.0 / 1024.0) << "MB"
                         << "in" << copyTime << "ms"
                         << "(" << speedMBps << "MB/s)";
            }
        }

        if (glaciosFileExists)
        {
            QString targetGlaciosFile = baseDatasetDir + "/Glacios_Data_Collection_Parameters.txt";
            QFileInfo glaciosInfo(glaciosFile);
            qint64 fileSize = glaciosInfo.size();

            QElapsedTimer copyTimer;
            copyTimer.start();

            if (QFile::exists(targetGlaciosFile)) {
                QFile::remove(targetGlaciosFile);
            }

            if (QFile::copy(glaciosFile, targetGlaciosFile))
            {
                qint64 copyTime = copyTimer.elapsed();
                double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                totalFilesCopied++;
                totalBytesCopied += fileSize;

                qDebug() << "    Glacios_Data_Collection_Parameters.txt -" << (fileSize / 1024.0 / 1024.0) << "MB"
                         << "in" << copyTime << "ms"
                         << "(" << speedMBps << "MB/s)";
            }
        }

        qDebug() << "\n  Saving graphs to:" << baseDatasetDir;

        QElapsedTimer graphTimer;

        for (int tabIndex = 0; tabIndex < ui->tabWidgetGraph->count(); ++tabIndex)
        {
            if (ui->tabWidgetGraph->tabText(tabIndex) == originalDataset)
            {
                QWidget *graphTab = ui->tabWidgetGraph->widget(tabIndex);
                QHBoxLayout *graphLayout = qobject_cast<QHBoxLayout*>(graphTab->layout());

                QWidget *totalDoseWidget = qobject_cast<QWidget*>(graphLayout->itemAt(0)->widget());
                QVBoxLayout *totalDoseLayout = qobject_cast<QVBoxLayout*>(totalDoseWidget->layout());
                //QCustomPlot *plotTotalDose = qobject_cast<QCustomPlot*>(totalDoseLayout->itemAt(0)->widget());

                QString dosePlotPath = baseDatasetDir + "/total_dose_plot.png";
                graphTimer.restart();
                //plotTotalDose->savePng(dosePlotPath, 1200, 800, 1.0, -1);
                qDebug() << "    total_dose_plot.png saved in" << graphTimer.elapsed() << "ms";

                QWidget *beamShiftsWidget = qobject_cast<QWidget*>(graphLayout->itemAt(1)->widget());
                QVBoxLayout *beamLayout = qobject_cast<QVBoxLayout*>(beamShiftsWidget->layout());
                //QCustomPlot *plotBeamShifts = qobject_cast<QCustomPlot*>(beamLayout->itemAt(1)->widget());

                QString beamPlotPath = baseDatasetDir + "/beam_shifts_plot.png";
                graphTimer.restart();
                //plotBeamShifts->savePng(beamPlotPath, 1200, 800, 1.0, -1);
                qDebug() << "    beam_shifts_plot.png saved in" << graphTimer.elapsed() << "ms";

                break;
            }
        }

        qDebug() << "\n  Saving table CSV to:" << baseDatasetDir;

        QElapsedTimer csvTimer;

        for (int tabIndex = 0; tabIndex < ui->tabWidgetFiles->count(); ++tabIndex)
        {
            if (ui->tabWidgetFiles->tabText(tabIndex) == originalDataset)
            {
                QWidget *tableTab = ui->tabWidgetFiles->widget(tabIndex);
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
                            {
                                headers << tableWidget->horizontalHeaderItem(col)->text();
                            }
                            stream << headers.join(",") << "\n";

                            for (int row = 0; row < tableWidget->rowCount(); ++row)
                            {
                                QStringList rowData;
                                for (int col = 0; col < tableWidget->columnCount(); ++col)
                                {
                                    QTableWidgetItem *item = tableWidget->item(row, col);
                                    QString cellData = item ? item->text() : "";
                                    if (cellData.contains(",") || cellData.contains("\""))
                                    {
                                        cellData = "\"" + cellData.replace("\"", "\"\"") + "\"";
                                    }
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

    for (const QString& originalDataset : selectedDatasets)
    {
        QString newDatasetName = datasetRenames[originalDataset];
        QString baseDatasetDir = destinationPath + "/" + newDatasetName;
        QString rawType = datasetRawType[originalDataset].toLower();
        QTableWidget *datasetTable = datasetTables[originalDataset];

        bool multiplePeaks = (rowNumbersPerPeak[originalDataset].size() > 1);

        for (auto it = rowNumbersPerPeak[originalDataset].constBegin(); it != rowNumbersPerPeak[originalDataset].constEnd(); ++it)
        {
            int peak = it.key();
            const QList<int>& rows = it.value();

            QString peakDir = multiplePeaks ? baseDatasetDir + QString("/peak%1").arg(peak) : baseDatasetDir;
            QString xmlDir = peakDir + "/data/xml";
            QString rawDir = peakDir + "/data/raw/" + rawType;
            QString movieDir = peakDir + "/data/movies";

            qDebug() << "\n  Peak" << peak << ":" << rows.size() << "files";

            for (int i = 0; i < rows.size(); ++i)
            {
                int row = rows[i];

                QString xmlPath = datasetTable->item(row, 9)->text();

                QString directory = datasetTable->item(row, 1)->text();
                QString filename = datasetTable->item(row, 2)->text();
                QString extension = (rawType == "eer") ? ".eer" : (rawType == "tiff" ? ".tiff" : ".mrc");
                QString rawPath = directory + "/" + filename + extension;

                int groupNumber = datasetTable->item(row, 23)->text().toInt();

                QFileInfo xmlInfo(xmlPath);
                QFileInfo rawInfo(rawPath);

                QElapsedTimer copyTimer;
                copyTimer.start();

                QString targetXml = xmlDir + "/" + xmlInfo.fileName();
                if (QFile::exists(targetXml)) QFile::remove(targetXml);

                if (QFile::copy(xmlPath, targetXml))
                {
                    qint64 copyTime = copyTimer.elapsed();
                    qint64 fileSize = xmlInfo.size();
                    double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                    qDebug() << "    [" << (i+1) << "/" << rows.size() << "] XML:" << xmlInfo.fileName() << (fileSize / 1024.0) << "KB" << copyTime << "ms" << speedMBps << "MB/s";

                    currentOperation++;
                    ui->progressBarMovies->setValue(currentOperation);
                }
                else
                {
                    qDebug() << "    FAILED XML:" << xmlInfo.fileName();
                }

                copyTimer.restart();

                QString targetRaw = rawDir + "/" + rawInfo.fileName();
                qDebug() << "    About to copy RAW:";
                qDebug() << "      FROM:" << rawPath;
                qDebug() << "      TO:" << targetRaw;
                qDebug() << "      SIZE:" << (rawInfo.size() / 1024.0 / 1024.0) << "MB";
                qDebug() << "      File exists:" << rawInfo.exists();

                if (QFile::copy(rawPath, targetRaw))
                {
                    qint64 copyTime = copyTimer.elapsed();
                    qint64 fileSize = rawInfo.size();
                    double speedMBps = (fileSize / 1024.0 / 1024.0) / (copyTime / 1000.0);
                    qDebug() << "    [" << (i+1) << "/" << rows.size() << "] RAW:" << rawInfo.fileName() << (fileSize / 1024.0 / 1024.0) << "MB" << copyTime << "ms" << speedMBps << "MB/s";

                    currentOperation++;
                    ui->progressBarMovies->setValue(currentOperation);

                    QString movieLinkName = QString("movie_%1_Group%2.%3").arg(i).arg(groupNumber).arg(rawType);
                    QString movieLinkPath = movieDir + "/" + movieLinkName;

                    if (QFile::link(targetRaw, movieLinkPath))
                    {
                        qDebug() << "    [" << (i+1) << "/" << rows.size() << "] LINK:" << movieLinkName;

                        currentOperation++;
                        ui->progressBarMovies->setValue(currentOperation);
                    }
                    else
                    {
                        qDebug() << "    FAILED LINK:" << movieLinkName;
                    }
                }
                else
                {
                    qDebug() << "    FAILED RAW:" << rawInfo.fileName();
                }
            }
        }
    }

    qDebug() << "Step 8: COMPLETED in" << stepTimer.elapsed() << "ms";


    QApplication::restoreOverrideCursor();
}

void MainWindow::clusterAndRecolorBeamShifts(QCustomPlot* plot)
{
    using namespace alglib;
    QElapsedTimer totalTimer, stepTimer;
    totalTimer.start();
    stepTimer.start();
    QWidget *parentWidget = plot->parentWidget();
    QLineEdit *textBox = parentWidget->findChild<QLineEdit*>("clustersTextBox");
    int numClusters = textBox->text().toInt();
    plot->clearItems();
    qDebug() << "=== ALGLIB K-MEANS CLUSTERING START ===";
    qDebug() << "Target clusters:" << numClusters;

    QCPCurve *beamCurve = plot->findChild<QCPCurve*>("beamCurve");
    if (!plot || !beamCurve) {
        qDebug() << "ERROR: Invalid plot or no beamCurve found";
        return;
    }

    // Step 1: Extract beam shift coordinates
    stepTimer.restart();
    QVector<DataPoint> points;
    for (auto it = beamCurve->data()->begin(); it != beamCurve->data()->end(); ++it) {
        points.append(DataPoint(it->key, it->value));
    }
    qDebug() << "STEP 1 - Extract points:" << stepTimer.elapsed() << "ms";
    qDebug() << "Total points extracted:" << points.size();

    if (points.size() < numClusters) {
        qDebug() << "ERROR: Not enough points for clustering. Need at least" << numClusters << "points";
        return;
    }

    // Step 2: Setup ALGLIB data matrix
    stepTimer.restart();
    real_2d_array data;
    data.setlength(points.size(), 2);
    for (int i = 0; i < points.size(); ++i) {
        data[i][0] = points[i].x;
        data[i][1] = points[i].y;
    }
    qDebug() << "STEP 2 - Setup ALGLIB data matrix:" << stepTimer.elapsed() << "ms";

    // Step 3: Run ALGLIB k-means
    stepTimer.restart();
    clusterizerstate s;
    kmeansreport rep;
    clusterizercreate(s);
    clusterizersetpoints(s, data, 2);
    clusterizersetkmeanslimits(s, 5, 100);
    clusterizerrunkmeans(s, numClusters, rep);
    qDebug() << "STEP 3 - K-means clustering:" << stepTimer.elapsed() << "ms";
    qDebug() << "Iterations:" << rep.iterationscount;

    // Generate distinct colors based on number of clusters
    QList<QRgb> generatedColors;
    for (int i = 0; i < numClusters; ++i) {
        double hue = (double(i) / numClusters) * 360.0;
        double saturation = (i % 2 == 0) ? 0.9 : 0.6;
        double value = (i % 3 == 0) ? 0.9 : 0.7;
        QColor color = QColor::fromHsvF(hue/360.0, saturation, value);
        generatedColors.append(color.rgb());
    }

    // Step 4: Plot results
    stepTimer.restart();
    plot->clearGraphs();
    coordinateToCluster.clear();

    for (int cluster = 0; cluster < numClusters; ++cluster) {
        plot->addGraph();
        QColor color(generatedColors[cluster]);
        plot->graph(cluster)->setPen(QPen(color, 3));
        plot->graph(cluster)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCross, color, color, 10));
        plot->graph(cluster)->setLineStyle(QCPGraph::lsNone);
    }

    for (int i = 0; i < points.size(); ++i) {
        int clusterID = rep.cidx[i];
        plot->graph(clusterID)->addData(points[i].x, points[i].y);
        coordinateToCluster[QPair<double, double>(points[i].x, points[i].y)] = clusterID + 1;
    }

    // Add cluster center labels
    for (int cluster = 0; cluster < numClusters; ++cluster) {
        double centerX = rep.c[cluster][0];
        double centerY = rep.c[cluster][1];
        QCPItemText *label = new QCPItemText(plot);
        label->setPositionAlignment(Qt::AlignCenter);
        label->position->setType(QCPItemPosition::ptPlotCoords);
        label->position->setCoords(centerX, centerY);
        label->setText(QString::number(cluster + 1));
        label->setFont(QFont("Arial", 10, QFont::Bold));
        label->setPen(QPen(Qt::black));
        label->setBrush(QBrush(Qt::white));
        label->setPadding(QMargins(2, 2, 2, 2));
    }

    plot->rescaleAxes();
    plot->replot();
    qDebug() << "STEP 4 - Plotting:" << stepTimer.elapsed() << "ms";
    qDebug() << "TOTAL TIME:" << totalTimer.elapsed() << "ms";
}

void MainWindow::on_pushButtonMoviesDirectory_clicked()
{
    QFileDialog dialog(this);
    dialog.setOptions(QFileDialog::HideNameFilterDetails | QFileDialog::DontUseNativeDialog);
    dialog.setDirectory("/home/data/raw3/globus/");
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

            QWidget *tabGraph = new QWidget();
            ui->tabWidgetGraph->addTab(tabGraph, datasetName);
            ui->tabWidgetGraph->setCurrentWidget(tabGraph);

            QCustomPlot *plotTotalDose = new QCustomPlot(tabGraph);
            plotTotalDose->plotLayout()->insertRow(0);
            QCPTextElement *titleTotalDose = new QCPTextElement(plotTotalDose, "Total Dose");
            titleTotalDose->setFont(QFont("sans", 16, QFont::Bold));
            plotTotalDose->plotLayout()->addElement(0, 0, titleTotalDose);
            plotTotalDose->xAxis->setLabel("Movie number");
            plotTotalDose->yAxis->setLabel("Total Dose (e/Ų)");

            QCustomPlot *plotBeamShifts = new QCustomPlot(tabGraph);
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

            QLabel *textLabelTotalDose = new QLabel(tabGraph);
            textLabelTotalDose->setFont(QFont(font().family(), 10));
            textLabelTotalDose->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            textLabelTotalDose->setWordWrap(true);
            textLabelTotalDose->setFrameStyle(QFrame::Box | QFrame::Plain);
            textLabelTotalDose->setMargin(5);
            textLabelTotalDose->setMaximumHeight(60);

            QVBoxLayout *totalDoseLayout = new QVBoxLayout();
            totalDoseLayout->addWidget(plotTotalDose);
            totalDoseLayout->addWidget(textLabelTotalDose);

            QWidget *totalDoseWidget = new QWidget();
            totalDoseWidget->setLayout(totalDoseLayout);

            QVBoxLayout *beamShiftsLayout = new QVBoxLayout();
            QLabel *beamShiftsLabel = new QLabel("Number of Clusters:", tabGraph);
            beamShiftsLabel->setFont(QFont("sans", 10, QFont::Bold));

            QLineEdit *beamShiftsTextBox = new QLineEdit(tabGraph);
            beamShiftsTextBox->setObjectName("clustersTextBox");
            beamShiftsTextBox->setMaximumHeight(25);
            beamShiftsTextBox->setMaximumWidth(50);
            beamShiftsTextBox->setText("15");
            beamShiftsTextBox->setValidator(new QIntValidator(1, 100, beamShiftsTextBox));

            QPushButton *recalculateButton = new QPushButton("Recalculate Clusters", tabGraph);
            connect(recalculateButton, &QPushButton::clicked, this, &MainWindow::onRecalculateClustersClicked);

            QHBoxLayout *beamShiftsInputLayout = new QHBoxLayout();
            beamShiftsInputLayout->addWidget(beamShiftsLabel);
            beamShiftsInputLayout->addWidget(beamShiftsTextBox);
            beamShiftsInputLayout->addWidget(recalculateButton);
            beamShiftsInputLayout->addStretch();

            beamShiftsLayout->addLayout(beamShiftsInputLayout);
            beamShiftsLayout->addWidget(plotBeamShifts);

            QWidget *beamShiftsWidget = new QWidget();
            beamShiftsWidget->setLayout(beamShiftsLayout);

            QHBoxLayout *layoutgraph = new QHBoxLayout(tabGraph);
            layoutgraph->addWidget(totalDoseWidget);
            layoutgraph->addWidget(beamShiftsWidget);
            layoutgraph->setStretch(0, 1);
            layoutgraph->setStretch(1, 1);

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
                    if (bc)
                        bc->addData(rowCount, xmlData.beamshiftX.toDouble(), xmlData.beamshiftY.toDouble());

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

            analyseAndFilterDose(tableWidget, plotTotalDose, textLabelTotalDose, 1.5);

            QElapsedTimer clusterTimer;
            clusterTimer.start();
            //clusterAndRecolorBeamShifts(plotBeamShifts);
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

MainWindow::DoseAnalysisResults MainWindow::analyseAndFilterDose(QTableWidget *tableWidget, QCustomPlot *plotTotalDose, QLabel *statsLabel, double minSeparation)
{
    DoseAnalysisResults results;
    results.minSeparation = minSeparation;
    if (!tableWidget || !plotTotalDose) return results;

    qDebug() << "=== START analyseAndFilterDose (minSeparation=" << minSeparation << ") ===";

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

    // Main = highest weight component
    int mainIdx = 0;
    for (int k = 1; k < bestK; ++k)
        if (components[k].weight > components[mainIdx].weight)
            mainIdx = k;

    double mainMean = components[mainIdx].mean;
    qDebug() << "Main component: mean=" << mainMean << "weight=" << components[mainIdx].weight;

    // Merge components within minSeparation of main
    QVector<bool> mergedIntoMain(bestK, false);
    mergedIntoMain[mainIdx] = true;

    for (int k = 0; k < bestK; ++k) {
        if (k == mainIdx) continue;
        double diff = qAbs(components[k].mean - mainMean);
        if (diff < minSeparation) {
            qDebug() << "Merging component k=" << k << "mean=" << components[k].mean << "diff=" << diff;
            mergedIntoMain[k] = true;
        } else {
            qDebug() << "Keeping separate component k=" << k << "mean=" << components[k].mean << "diff=" << diff;
        }
    }

    for (int k = 0; k < bestK; ++k)
        components[k].isMain = mergedIntoMain[k];

    int numPopulations = 1;
    for (int k = 0; k < bestK; ++k)
        if (!components[k].isMain) numPopulations++;

    qDebug() << "Total populations:" << numPopulations;

    // --- 4. Generate colours for outlier components ---
    QVector<QColor> rejectedColors(bestK);
    int outlierIdx = 0;
    for (int k = 0; k < bestK; ++k) {
        if (!components[k].isMain) {
            double hue = (double(outlierIdx) / qMax(numPopulations - 1, 1)) * 300.0;
            rejectedColors[k] = QColor::fromHsvF(hue/360.0, 0.9, 0.8);
            outlierIdx++;
        }
    }

    // --- 5. Assign keep/reject ---
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
        int row   = validRows[i];
        double dose = doseValues[i];
        int comp  = compAssignment[i];
        bool keep = components[comp].isMain;

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

        // Column 24: population index (1=main, 2+ = outliers sorted by mean descending)
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

    // --- 6. Plot ---
    plotTotalDose->clearGraphs();
    plotTotalDose->clearItems();

    int graphIdx = 0;

    // All kept points - single graph, single colour
    if (!xKept.isEmpty()) {
        plotTotalDose->addGraph();
        plotTotalDose->graph(graphIdx)->setPen(QPen(QColor(0, 100, 200)));
        plotTotalDose->graph(graphIdx)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 4));
        plotTotalDose->graph(graphIdx)->setLineStyle(QCPGraph::lsNone);
        plotTotalDose->graph(graphIdx)->setData(xKept, yKept);
        plotTotalDose->graph(graphIdx)->setName("Kept");
        graphIdx++;
    }

    // Rejected points per outlier component
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

    // Threshold line at mainMean - minSeparation
    double threshLineY = mainMean - minSeparation;
    QCPItemLine *line = new QCPItemLine(plotTotalDose);
    line->start->setType(QCPItemPosition::ptPlotCoords);
    line->start->setCoords(0, threshLineY);
    line->end->setType(QCPItemPosition::ptPlotCoords);
    line->end->setCoords(tableWidget->rowCount(), threshLineY);
    line->setPen(QPen(Qt::red, 1, Qt::DashLine));

    plotTotalDose->xAxis->setRange(-5, tableWidget->rowCount() + 5);
    plotTotalDose->yAxis->setRange(minD - 2.0, maxD + 2.0);

    plotTotalDose->legend->setVisible(true);
    plotTotalDose->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignBottom | Qt::AlignLeft);
    plotTotalDose->legend->setFont(QFont("sans", 8));
    plotTotalDose->replot();

    // --- 7. Stats label ---
    if (statsLabel) {
        QString statsText = QString("GMM: %1 raw components, %2 populations (sep≥%3 e/Ų) | Kept: %4 (mean=%5 e/Ų) | Rejected: %6")
                                .arg(bestK)
                                .arg(numPopulations)
                                .arg(minSeparation, 0, 'f', 1)
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

    // --- 8. Results ---
    results.numPopulations = numPopulations;
    results.mainMean       = mainMean;
    results.mainStd        = components[mainIdx].std;
    results.keptCount      = keptCount;
    results.rejectedCount  = rejectedCount;
    results.keptMeanDose   = keptMean;

    qDebug() << "Kept=" << keptCount << "Rejected=" << rejectedCount << "Mean(kept)=" << keptMean;
    qDebug() << "=== END analyseAndFilterDose ===";
    return results;
}
