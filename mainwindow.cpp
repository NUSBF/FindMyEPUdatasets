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
    MovieFilter << "*.xml" << "*.eer" << "*.tiff" << "*.mrc";

    // Count total files for progress bar
    int totalFiles = 0;
    QDirIterator counter(path, MovieFilter, QDir::AllEntries | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (counter.hasNext()) {
        counter.next();
        if (counter.fileInfo().isFile()) totalFiles++;
    }

    ui->progressBarMovies->setMaximum(totalFiles);
    ui->progressBarMovies->setMinimum(0);
    ui->progressBarMovies->setValue(0);

    // Process all files
    int xmlCount = 0, eerCount = 0, tiffCount = 0, mrcCount = 0;
    QStringList allXMLBasenames;
    qint64 minFileSize = 100 * 1024 * 1024; // 100MB for MRC filtering

    QDirIterator itAllFiles(path, MovieFilter, QDir::AllEntries | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    int processedFiles = 0;

    while (itAllFiles.hasNext())
    {
        itAllFiles.next();
        if (itAllFiles.fileInfo().isFile())
        {
            processedFiles++;
            ui->progressBarMovies->setValue(processedFiles);

            QString fullPath = itAllFiles.filePath();
            QStringList pathParts = fullPath.split("/");
            int imagesDiscIndex = pathParts.indexOf("Images-Disc1");

            if (imagesDiscIndex > 0)
            {
                QString extension = itAllFiles.fileInfo().suffix().toLower();

                // Skip small MRC files
                if (extension == "mrc" && itAllFiles.fileInfo().size() < minFileSize)
                {
                    continue;
                }

                // Normalize dataset name to handle inconsistencies
                QString datasetName = pathParts[imagesDiscIndex - 1];
                datasetName = datasetName.trimmed();
                datasetName = datasetName.replace(" ", "_");
                if (datasetName.endsWith("_")) {
                    datasetName = datasetName.left(datasetName.length() - 1);
                }

                FileInfo info;
                info.datasetName = datasetName;
                info.fileName = itAllFiles.fileName();
                info.extension = extension;
                info.baseName = QFileInfo(itAllFiles.fileName()).baseName();
                info.filePath = fullPath;
                allFiles.append(info);

                // Count file types and collect XML basenames
                if (extension == "xml") {
                    xmlCount++;
                    allXMLBasenames.append(info.baseName);
                } else if (extension == "eer") {
                    eerCount++;
                } else if (extension == "tiff") {
                    tiffCount++;
                } else if (extension == "mrc") {
                    mrcCount++;
                }
            }
        }
    }

    ui->labelXMLcount->setText(QString("%0 XML files found").arg(xmlCount));
    ui->labelEERcount->setText(QString("%0 EER files found").arg(eerCount));
    ui->labelTIFFcount->setText(QString("%0 TIFF files found").arg(tiffCount));
    ui->labelMRCcount->setText(QString("%0 MRC files found").arg(mrcCount));

    // Find matches using contains logic
    QMap<QString, int> xmlCounts;
    QMap<QString, int> eerMatches, tiffMatches, mrcMatches;
    QMap<QString, QString> extensions;

    // Count XMLs by dataset
    for (const auto& file : allFiles)
    {
        if (file.extension == "xml")
        {
            xmlCounts[file.datasetName]++;
        }
    }


    // Find matching image files
    int totalMatches = 0;
    for (const auto& file : allFiles)
    {
        if (file.extension != "xml")
        {
            // Check if filename contains any XML basename
            bool foundMatch = false;
            for (const QString& xmlBaseName : allXMLBasenames)
            {
                if (file.fileName.contains(xmlBaseName))
                {
                    foundMatch = true;
                    break;
                }
            }

            if (foundMatch)
            {
                totalMatches++;

                if (file.extension == "eer") eerMatches[file.datasetName]++;
                else if (file.extension == "tiff") tiffMatches[file.datasetName]++;
                else if (file.extension == "mrc") mrcMatches[file.datasetName]++;

                // Set extension priority: EER > TIFF > MRC
                QString currentExt = extensions[file.datasetName];
                if (currentExt.isEmpty() ||
                    (file.extension == "eer") ||
                    (file.extension == "tiff" && currentExt != "EER"))
                {
                    extensions[file.datasetName] = file.extension.toUpper();
                }
            }
        }
    }

    // Debug output
    qDebug() << "TOTAL FILE COUNTS - XML:" << xmlCount << "EER:" << eerCount << "TIFF:" << tiffCount << "MRC:" << mrcCount;
    qDebug() << "Total matches found:" << totalMatches;

    // Calculate total matches per dataset
    QMap<QString, int> totalMatchesByDataset;
    for (auto it = xmlCounts.begin(); it != xmlCounts.end(); ++it)
    {
        QString dataset = it.key();
        int total = eerMatches[dataset] + tiffMatches[dataset] + mrcMatches[dataset];
        if (total > 0) {
            totalMatchesByDataset[dataset] = total;
            qDebug() << "Dataset:" << dataset
                     << "EER:" << eerMatches[dataset]
                     << "TIFF:" << tiffMatches[dataset]
                     << "MRC:" << mrcMatches[dataset]
                     << "Extension:" << extensions[dataset];
        }
    }

    // Populate table using the SAME data as debug
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
        checkbox->setChecked(it.value() > 500);
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

        QString extension = extensions.value(it.key(), "NONE");
        QTableWidgetItem *extensionItem = new QTableWidgetItem(extension);
        extensionItem->setFlags(extensionItem->flags() ^ Qt::ItemIsEditable);
        extensionItem->setTextAlignment(Qt::AlignCenter);
        ui->tableWidgetDatasets->setItem(row, 4, extensionItem);

        row++;
    }
    ui->tableWidgetDatasets->resizeColumnsToContents();

    // Create tabs for SELECTED datasets only
    ui->tabWidgetFiles->clear();
    ui->tabWidget->setCurrentIndex(2);
    ui->tabWidgetGraph->clear();

    int totalSelectedXMLFiles = 0;
    int processedXMLFiles = ui->progressBarMovies->value();
    qDebug() << "Processed XML files is set at " << processedXMLFiles;


    for (int i = 0; i < ui->tableWidgetDatasets->rowCount(); ++i) {
        QCheckBox *checkbox = qobject_cast<QCheckBox*>(ui->tableWidgetDatasets->cellWidget(i, 0));
        if (checkbox && checkbox->isChecked()) {
            totalSelectedXMLFiles += ui->tableWidgetDatasets->item(i, 2)->text().toInt(); // Column 2 is XML count
        }
    }
    ui->progressBarMovies->setMaximum(ui->progressBarMovies->maximum()+totalSelectedXMLFiles);
    qDebug() << "Progress BAr max is set at " << ui->progressBarMovies->maximum();

    for (int i = 0; i < ui->tableWidgetDatasets->rowCount(); ++i)
    {


        QCheckBox *checkbox = qobject_cast<QCheckBox*>(ui->tableWidgetDatasets->cellWidget(i, 0));
        if (checkbox && checkbox->isChecked())
        {


            QString datasetName = ui->tableWidgetDatasets->item(i, 1)->text();
            QString chosenExtension = extensions[datasetName].toLower();

            QWidget *tab = new QWidget();
            ui->tabWidgetFiles->addTab(tab, datasetName);
            ui->tabWidgetFiles->setCurrentWidget(tab);



            QTableWidget *tableWidget = new QTableWidget(tab);
            tableWidget->setColumnCount(21);
            tableWidget->setSortingEnabled(true);
            QStringList headerList;
            headerList << "0: Item"
                       << "1: Movie file path"
                       << "2: Movie file name"
                       << "3: Movie file size"
                       << "4: Movie last modified time"
                       << "5: XML file name"
                       << "6: XML file path"
                       << "7: XML file size"
                       << "8: XML last modified time"
                       << "9: XML acquisition time"
                       << "10: Estimated movie acquisition rate"
                       << "11: Voltage"
                       << "12: Beamshift X"
                       << "13: Beamshift Y"
                       << "14: Beamtilt X"
                       << "15: BeamTilt Y"
                       << "16: Exposure time"
                       << "17: Pixel Size"
                       << "18: Dose per Pixel"
                       << "19: Total Dose"
                       << "20: Group";
            tableWidget->setHorizontalHeaderLabels(headerList);

            QVBoxLayout *layout = new QVBoxLayout(tab);
            layout->addWidget(tableWidget);

            QWidget *tabGraph = new QWidget();
            ui->tabWidgetGraph->addTab(tabGraph, datasetName);
            ui->tabWidgetGraph->setCurrentWidget(tabGraph);

            // Create two QCustomPlot widgets
            QCustomPlot *plotTotalDose = new QCustomPlot(tabGraph);
            QCustomPlot *plotBeamShifts = new QCustomPlot(tabGraph);

            // Create layout to split tab in half
            QHBoxLayout *layoutgraph = new QHBoxLayout(tabGraph);
            layoutgraph->addWidget(plotTotalDose);
            layoutgraph->addWidget(plotBeamShifts);
            plotTotalDose->addGraph();
            plotTotalDose->graph(0)->setPen(QPen(Qt::red));
            plotBeamShifts->addGraph();
            plotBeamShifts->graph(0)->setScatterStyle(QCPScatterStyle::ssCross);
            plotBeamShifts->graph(0)->setLineStyle(QCPGraph::lsNone);
            plotBeamShifts->replot();

            // Set graph titles
            plotTotalDose->plotLayout()->insertRow(0);
            QCPTextElement *titleTotalDose = new QCPTextElement(plotTotalDose, "Total Dose");
            titleTotalDose->setFont(QFont("sans", 16, QFont::Bold)); // Size 16, bold
            plotTotalDose->plotLayout()->addElement(0, 0, titleTotalDose);

            plotBeamShifts->plotLayout()->insertRow(0);
            QCPTextElement *titleBeamShifts = new QCPTextElement(plotBeamShifts, "Beam Shifts");
            titleBeamShifts->setFont(QFont("sans", 16, QFont::Bold)); // Size 16, bold
            plotBeamShifts->plotLayout()->addElement(0, 0, titleBeamShifts);

            //set qcustomplot graph dose
            plotTotalDose->xAxis->setLabel("Movie number");
            plotTotalDose->yAxis->setLabel("Total Dose");


            QCPItemText *textLabelTotalDose = new QCPItemText(plotTotalDose);
            textLabelTotalDose->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
            textLabelTotalDose->position->setType(QCPItemPosition::ptAxisRectRatio);
            textLabelTotalDose->position->setCoords(0.5, 0.05);
            textLabelTotalDose->setFont(QFont(font().family(), 12));
            textLabelTotalDose->setPen(QPen(Qt::black));
            plotTotalDose->replot();

            //set qcustomplot beam shifts
            plotBeamShifts->xAxis->setLabel("Beam shift X (A.U.)");
            plotBeamShifts->yAxis->setLabel("Beam shift Y");
            plotBeamShifts->replot();


            QVector<double> doseValues;
            int row = 0;
            for (const auto& xmlFile : allFiles)
            {

                if (xmlFile.datasetName == datasetName && xmlFile.extension == "xml")
                {

                    processedXMLFiles++;
                    ui->progressBarMovies->setValue(processedXMLFiles);
                    //qDebug() << processedXMLFiles;
                    //qDebug().noquote().nospace() << "Forced: " << processedXMLFiles;
                    //QApplication::processEvents();
                    QFileInfo xmlFileInfo(xmlFile.filePath);

                    // Find matching movie file
                    QString matchingMovieFile = "";
                    QFileInfo movieFileInfo;
                    for (const auto& movieFile : allFiles) {
                        if (movieFile.datasetName == datasetName &&
                            movieFile.extension == chosenExtension &&
                            movieFile.fileName.contains(xmlFile.baseName)) {
                            matchingMovieFile = movieFile.filePath;
                            movieFileInfo = QFileInfo(movieFile.filePath);
                            break;
                        }
                    }

                    QString tmprow = QString("%0").arg(row + 1);
                    QTableWidgetItem *item = new QTableWidgetItem(tmprow);

                    // Movie file info (columns 1-4)
                    QTableWidgetItem *moviePathItem = new QTableWidgetItem(matchingMovieFile);
                    moviePathItem->setFlags(moviePathItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *movieNameItem = new QTableWidgetItem(movieFileInfo.fileName());
                    movieNameItem->setFlags(movieNameItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *movieSizeItem = new QTableWidgetItem(QLocale().formattedDataSize(movieFileInfo.size()) + ".");
                    movieSizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    movieSizeItem->setFlags(movieSizeItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *movieModifiedItem = new QTableWidgetItem(QDateTime(movieFileInfo.lastModified()).toString());
                    movieModifiedItem->setFlags(movieModifiedItem->flags() ^ Qt::ItemIsEditable);

                    // XML file info (columns 5-8)
                    QTableWidgetItem *xmlNameItem = new QTableWidgetItem(xmlFile.fileName);
                    xmlNameItem->setFlags(xmlNameItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *xmlPathItem = new QTableWidgetItem(xmlFile.filePath);
                    xmlPathItem->setFlags(xmlPathItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *xmlSizeItem = new QTableWidgetItem(QLocale().formattedDataSize(xmlFileInfo.size()) + ".");
                    xmlSizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    xmlSizeItem->setFlags(xmlSizeItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *xmlModifiedItem = new QTableWidgetItem(QDateTime(xmlFileInfo.lastModified()).toString());
                    xmlModifiedItem->setFlags(xmlModifiedItem->flags() ^ Qt::ItemIsEditable);


                    // Parse XML metadata (columns 9-19)
                    XMLData xmlData = parseXMLFile(xmlFile.filePath);

                    QTableWidgetItem *acquisitionTimeItem = new QTableWidgetItem(xmlData.acquisitionTime);
                    acquisitionTimeItem->setFlags(acquisitionTimeItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *movieRateItem = new QTableWidgetItem(xmlData.movieAcquisitionRate);
                    movieRateItem->setFlags(movieRateItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *voltageItem = new QTableWidgetItem(xmlData.voltage);
                    voltageItem->setFlags(voltageItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *beamshiftXItem = new QTableWidgetItem(xmlData.beamshiftX);
                    beamshiftXItem->setFlags(beamshiftXItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *beamshiftYItem = new QTableWidgetItem(xmlData.beamshiftY);
                    beamshiftYItem->setFlags(beamshiftYItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *beamtiltXItem = new QTableWidgetItem(xmlData.beamtiltX);
                    beamtiltXItem->setFlags(beamtiltXItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *beamtiltYItem = new QTableWidgetItem(xmlData.beamtiltY);
                    beamtiltYItem->setFlags(beamtiltYItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *exposureTimeItem = new QTableWidgetItem(xmlData.exposureTime);
                    exposureTimeItem->setFlags(exposureTimeItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *pixelSizeItem = new QTableWidgetItem(xmlData.pixelSize);
                    pixelSizeItem->setFlags(pixelSizeItem->flags() ^ Qt::ItemIsEditable);

                    QTableWidgetItem *dosePerPixelItem = new QTableWidgetItem(xmlData.dosePerPixel);
                    dosePerPixelItem->setFlags(dosePerPixelItem->flags() ^ Qt::ItemIsEditable);

                    // Calculate total dose
                    QString totalDose = "";
                    double totalDoseValue =0;
                    if (!xmlData.pixelSize.isEmpty() && !xmlData.dosePerPixel.isEmpty()) {
                        double pixelValue = xmlData.pixelSize.toDouble();
                        pixelValue = pixelValue * pixelValue;
                        double doseValue = xmlData.dosePerPixel.toDouble();
                        totalDoseValue = doseValue / pixelValue;
                        totalDose = QString("%0").arg(totalDoseValue);
                        doseValues.append(totalDoseValue);
                    }
                    QTableWidgetItem *totalDoseItem = new QTableWidgetItem(totalDose);
                    totalDoseItem->setFlags(totalDoseItem->flags() ^ Qt::ItemIsEditable);

                    plotTotalDose->graph(0)->addData(row,totalDoseValue);
                    plotTotalDose->xAxis->setRange(-5,row+5);
                    bool foundRange;
                    QCPRange valueRange = plotTotalDose->graph(0)->data()->valueRange(foundRange);
                    plotTotalDose->yAxis->setRange(0,valueRange.upper+20);

                    //textLabelTotalDose->setText(QString("The lowest dose is %0 e/A^2, the average dose is %1 e/A^2 and the highest dose is %3 e/A^2.").arg(valueRange.lower).arg(meandose).arg(valueRange.upper));
                    plotTotalDose->replot();

                    plotBeamShifts->graph(0)->addData(xmlData.beamshiftX.toDouble(),xmlData.beamshiftY.toDouble());


                    QCPRange valueRangeBeamShiftsX = plotBeamShifts->graph(0)->data()->keyRange(foundRange);
                    plotBeamShifts->xAxis->setRange(valueRangeBeamShiftsX.lower-0.1,valueRangeBeamShiftsX.upper+0.1);
                    QCPRange valueRangeBeamShiftsY = plotBeamShifts->graph(0)->data()->valueRange(foundRange);
                    plotBeamShifts->yAxis->setRange(valueRangeBeamShiftsY.lower-0.1,valueRangeBeamShiftsY.upper+0.1);

                    tableWidget->insertRow(row);
                    tableWidget->setItem(row, 0, item);
                    tableWidget->setItem(row, 1, moviePathItem);
                    tableWidget->setItem(row, 2, movieNameItem);
                    tableWidget->setItem(row, 3, movieSizeItem);
                    tableWidget->setItem(row, 4, movieModifiedItem);
                    tableWidget->setItem(row, 5, xmlNameItem);
                    tableWidget->setItem(row, 6, xmlPathItem);
                    tableWidget->setItem(row, 7, xmlSizeItem);
                    tableWidget->setItem(row, 8, xmlModifiedItem);
                    tableWidget->setItem(row, 9, acquisitionTimeItem);
                    tableWidget->setItem(row, 10, movieRateItem);
                    tableWidget->setItem(row, 11, voltageItem);
                    tableWidget->setItem(row, 12, beamshiftXItem);
                    tableWidget->setItem(row, 13, beamshiftYItem);
                    tableWidget->setItem(row, 14, beamtiltXItem);
                    tableWidget->setItem(row, 15, beamtiltYItem);
                    tableWidget->setItem(row, 16, exposureTimeItem);
                    tableWidget->setItem(row, 17, pixelSizeItem);
                    tableWidget->setItem(row, 18, dosePerPixelItem);
                    tableWidget->setItem(row, 19, totalDoseItem);
                    row++;
                }


                // Calculate statistics - find the MEDIAN (central peak value)
                double peakDose = 0.0;
                double stdDevDose = 0.0;

                if (!doseValues.isEmpty()) {
                    // Sort the values to find median
                    QVector<double> sortedDoses = doseValues;
                    std::sort(sortedDoses.begin(), sortedDoses.end());

                    // Find median (middle value)
                    int size = sortedDoses.size();
                    if (size % 2 == 0) {
                        peakDose = (sortedDoses[size/2 - 1] + sortedDoses[size/2]) / 2.0;
                    } else {
                        peakDose = sortedDoses[size/2];
                    }

                    // Calculate standard deviation
                    double variance = 0.0;
                    for (double dose : doseValues) {
                        variance += (dose - peakDose) * (dose - peakDose);
                    }
                    variance = variance / doseValues.size();
                    stdDevDose = qSqrt(variance);
                }

                // Update text
                bool foundRange;
                QCPRange valueRange = plotTotalDose->graph(0)->data()->valueRange(foundRange);
                textLabelTotalDose->setText(QString("Lowest Dose: %0 e/A²\nMode Dose: %1 e/A²\nHighest Dose: %2 e/A²\nStd Dev: %3 e/A²")
                                                .arg(valueRange.lower, 0, 'f', 2)
                                                .arg(peakDose, 0, 'f', 2)
                                                .arg(valueRange.upper, 0, 'f', 2)
                                                .arg(stdDevDose, 0, 'f', 2));
                plotTotalDose->replot();




            }

            QElapsedTimer clusterTimer;
            clusterTimer.start();
            clusterAndRecolorBeamShifts(plotBeamShifts, listrgb);
            qDebug() << "Clustering took:" << clusterTimer.elapsed() << "ms";

            // Fill Group column in table
            for (int tableRow = 0; tableRow < tableWidget->rowCount(); ++tableRow)
            {
                double beamX = tableWidget->item(tableRow, 12)->text().toDouble();
                double beamY = tableWidget->item(tableRow, 13)->text().toDouble();

                int groupNumber = coordinateToCluster.value(QPair<double, double>(beamX, beamY), 0);

                QTableWidgetItem *groupItem = new QTableWidgetItem(QString::number(groupNumber));
                groupItem->setFlags(groupItem->flags() ^ Qt::ItemIsEditable);
                groupItem->setTextAlignment(Qt::AlignCenter);
                tableWidget->setItem(tableRow, 20, groupItem);
            }
            tableWidget->resizeColumnsToContents();
            qDebug() << "Created tab for selected dataset:" << datasetName << "with" << row << "XML files";
        }
    }

    QString formattedTime = QTime::fromMSecsSinceStartOfDay(timer.elapsed()).toString("hh:mm:ss.zzz");
    qDebug() << formattedTime;
    ui->labelFindingTime->setText("Finding Time: "+formattedTime);
    ui->pushButtonDestinationDirectory->setEnabled(true);
    QApplication::restoreOverrideCursor();
}

void MainWindow::on_pushButtonDestinationDirectory_clicked()
{
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
        return;
    }


    QString destinationPath = ListOfFiles[0];

    QElapsedTimer timer;
    timer.start();
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Get selected datasets from table
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
        return;
    }

    if (allFiles.isEmpty())
    {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, "No Data", "No file data available. Please scan directories first.");
        return;
    }

    // Get new names for each selected dataset
    QMap<QString, QString> datasetRenames;
    for (const QString& dataset : selectedDatasets)
    {
        bool ok;
        QDialog renameDialog(this);
        renameDialog.setWindowTitle("Rename Dataset");
        renameDialog.setMinimumWidth(qMax(400, dataset.length() * 8)); // Adjust multiplier as needed

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

        ok = (renameDialog.exec() == QDialog::Accepted);
        QString newName = ok ? lineEdit->text() : QString();
        if (!ok)
        {
            QApplication::restoreOverrideCursor();
            return; // User cancelled
        }
        datasetRenames[dataset] = newName;
    }

    QString sourceDir = ui->labelSelectedDirectoryMovie->text();
    QString glaciosFile = "";
    bool glaciosFileExists = false;
    QDirIterator glaciosIterator(sourceDir, QStringList() << "Glacios_Data_Collection_Parameters.txt", QDir::Files, QDirIterator::Subdirectories);
    if (glaciosIterator.hasNext())
    {
        glaciosIterator.next();
        glaciosFile = glaciosIterator.filePath();
        glaciosFileExists = true;
        qDebug() << "Found Glacios_Data_Collection_Parameters.txt at:" << glaciosFile;
    }
    else
    {
        qDebug() << "Glacios_Data_Collection_Parameters.txt not found in source directory tree";
    }

    // Process each selected dataset
    int totalMoved = 0;
    for (const QString& originalDataset : selectedDatasets)
    {
        QString newDatasetName = datasetRenames[originalDataset];

        // Create directory structure
        QString datasetDir = destinationPath + "/" + newDatasetName;
        QString dataDir = datasetDir + "/data";
        QString rawDir = dataDir + "/raw";
        QString xmlDir = dataDir + "/xml";
        QString moviesDir = dataDir + "/movies";

        if (!QDir().mkpath(rawDir)) {
            qDebug() << "FAILED to create:" << rawDir;
            continue;
        }
        if (!QDir().mkpath(xmlDir)) {
            qDebug() << "FAILED to create:" << xmlDir;
            continue;
        }
        if (!QDir().mkpath(moviesDir)) {
            qDebug() << "FAILED to create:" << moviesDir;
            continue;
        }

        if (glaciosFileExists) {
            QString targetGlaciosFile = datasetDir + "/Glacios_Data_Collection_Parameters.txt";
            if (QFile::copy(glaciosFile, targetGlaciosFile)) {
                qDebug() << "Copied Glacios_Data_Collection_Parameters.txt to" << newDatasetName;
            } else {
                qDebug() << "FAILED to copy Glacios_Data_Collection_Parameters.txt to" << newDatasetName;
            }
        }

        qDebug() << "Created directories for:" << newDatasetName;

        // SAVE GRAPHS AND TABLE DATA
        // Find the corresponding tabs for this dataset
        for (int tabIndex = 0; tabIndex < ui->tabWidgetGraph->count(); ++tabIndex) {
            if (ui->tabWidgetGraph->tabText(tabIndex) == originalDataset) {
                // Found the graph tab
                QWidget *graphTab = ui->tabWidgetGraph->widget(tabIndex);
                QHBoxLayout *graphLayout = qobject_cast<QHBoxLayout*>(graphTab->layout());

                if (graphLayout && graphLayout->count() >= 2) {
                    // Get the two plots
                    QCustomPlot *plotTotalDose = qobject_cast<QCustomPlot*>(graphLayout->itemAt(0)->widget());
                    QCustomPlot *plotBeamShifts = qobject_cast<QCustomPlot*>(graphLayout->itemAt(1)->widget());

                    if (plotTotalDose) {
                        QString dosePlotPath = datasetDir + "/total_dose_plot.png";
                        plotTotalDose->savePng(dosePlotPath, 1200, 800, 1.0, -1);
                        qDebug() << "Saved total dose plot:" << dosePlotPath;
                    }

                    if (plotBeamShifts) {
                        QString beamPlotPath = datasetDir + "/beam_shifts_plot.png";
                        plotBeamShifts->savePng(beamPlotPath, 1200, 800, 1.0, -1);
                        qDebug() << "Saved beam shifts plot:" << beamPlotPath;
                    }
                }
                break;
            }
        }

        // Find and save table data as CSV
        for (int tabIndex = 0; tabIndex < ui->tabWidgetFiles->count(); ++tabIndex) {
            if (ui->tabWidgetFiles->tabText(tabIndex) == originalDataset) {
                QWidget *tableTab = ui->tabWidgetFiles->widget(tabIndex);
                QVBoxLayout *tableLayout = qobject_cast<QVBoxLayout*>(tableTab->layout());

                if (tableLayout && tableLayout->count() >= 1) {
                    QTableWidget *tableWidget = qobject_cast<QTableWidget*>(tableLayout->itemAt(0)->widget());

                    if (tableWidget) {
                        QString csvPath = datasetDir + "/dataset_info.csv";
                        QFile csvFile(csvPath);

                        if (csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                            QTextStream stream(&csvFile);

                            // Write headers
                            QStringList headers;
                            for (int col = 0; col < tableWidget->columnCount(); ++col) {
                                headers << tableWidget->horizontalHeaderItem(col)->text();
                            }
                            stream << headers.join(",") << "\n";

                            // Write data rows
                            for (int row = 0; row < tableWidget->rowCount(); ++row) {
                                QStringList rowData;
                                for (int col = 0; col < tableWidget->columnCount(); ++col) {
                                    QTableWidgetItem *item = tableWidget->item(row, col);
                                    QString cellData = item ? item->text() : "";
                                    if (cellData.contains(",") || cellData.contains("\"")) {
                                        cellData = "\"" + cellData.replace("\"", "\"\"") + "\"";
                                    }
                                    rowData << cellData;
                                }
                                stream << rowData.join(",") << "\n";
                            }
                            csvFile.close();
                            qDebug() << "Saved CSV file:" << csvPath;
                        }
                    }
                }
                break;
            }
        }



        // Move files
        int datasetMoved = 0;
        for (const auto& file : allFiles)
        {
            if (file.datasetName == originalDataset)
            {
                // Get path after Images-Disc1
                QStringList pathParts = file.filePath.split("/");
                int imagesDiscIndex = pathParts.indexOf("Images-Disc1");
                if (imagesDiscIndex == -1) continue;

                QStringList subPath = pathParts.mid(imagesDiscIndex + 1);
                subPath.removeLast(); // Remove filename
                QString subDirectory = subPath.join("/");

                if (file.extension == "xml")
                {
                    // Move XML files
                    QString targetDir = xmlDir;
                    if (!subDirectory.isEmpty()) {
                        targetDir += "/" + subDirectory;
                        QDir().mkpath(targetDir);
                    }
                    QString targetFile = targetDir + "/" + file.fileName;

                    if (QFile::rename(file.filePath, targetFile))
                    {
                        datasetMoved++;
                        totalMoved++;
                    }
                    else
                    {
                        qDebug() << "FAILED to move XML:" << file.fileName << "from" << file.filePath << "to" << targetFile;
                    }
                }
                else if (file.extension == "eer" || file.extension == "tiff" || file.extension == "mrc")
                {
                    // Move image files
                    QString targetDir = rawDir;
                    if (!subDirectory.isEmpty()) {
                        targetDir += "/" + subDirectory;
                        QDir().mkpath(targetDir);
                    }
                    QString targetFile = targetDir + "/" + file.fileName;
                    if (QFile::rename(file.filePath, targetFile))
                    {
                        datasetMoved++;
                        totalMoved++;

                        // Get group number from table
                        int groupNumber = 0;
                        for (int tabIndex = 0; tabIndex < ui->tabWidgetFiles->count(); ++tabIndex) {
                            if (ui->tabWidgetFiles->tabText(tabIndex) == originalDataset) {
                                QTableWidget *tableWidget = qobject_cast<QTableWidget*>(ui->tabWidgetFiles->widget(tabIndex)->layout()->itemAt(0)->widget());
                                for (int row = 0; row < tableWidget->rowCount(); ++row) {
                                    if (tableWidget->item(row, 2)->text() == file.fileName) {
                                        groupNumber = tableWidget->item(row, 20)->text().toInt();
                                        break;
                                    }
                                }
                                break;
                            }
                        }

                        QFileInfo fileInfo(file.fileName);
                        QString linkFileName = QString("%1_group%2.%3").arg(fileInfo.baseName()).arg(groupNumber, 2, 10, QChar('0')).arg(fileInfo.suffix());
                        QString linkPath = moviesDir + "/" + linkFileName;

                        if (!QFile::link(targetFile, linkPath)) {
                            qDebug() << "FAILED to create link:" << linkPath;
                        }
                    }
                    else
                    {
                        qDebug() << "FAILED to move image:" << file.fileName << "from" << file.filePath << "to" << targetFile;
                    }
                }
            }
        }
        qDebug() << "Dataset" << originalDataset << "moved" << datasetMoved << "files";
    }

    QString formattedTime = QTime::fromMSecsSinceStartOfDay(timer.elapsed()).toString("hh:mm:ss.zzz");
    qDebug() << "Move completed in:" << formattedTime << "Total files moved:" << totalMoved;
    QApplication::restoreOverrideCursor();

    if (totalMoved > 0) {
        QMessageBox::information(this, "Complete", QString("Successfully moved %1 files!").arg(totalMoved));
    } else {
        QMessageBox::warning(this, "Failed", "No files were moved. Check debug output for errors.");
    }

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

double MainWindow::calculateDistance(const DataPoint& p1, const DataPoint& p2)
{
    return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

QVector<QVector<MainWindow::DataPoint>> MainWindow::kMeansClustering(QVector<DataPoint>& points, int k, int maxIterations)
{
    if (points.isEmpty() || k <= 0) return {};

    QVector<DataPoint> centroids;
    for (int i = 0; i < k; ++i) {
        centroids.append(points[i % points.size()]);
    }

    for (int iter = 0; iter < maxIterations; ++iter) {
        for (auto& point : points) {
            double minDist = std::numeric_limits<double>::max();
            int bestCluster = 0;

            for (int i = 0; i < centroids.size(); ++i) {
                double dist = calculateDistance(point, centroids[i]);
                if (dist < minDist) {
                    minDist = dist;
                    bestCluster = i;
                }
            }
            point.cluster = bestCluster;
        }

        QVector<DataPoint> newCentroids;
        for (int i = 0; i < k; ++i) {
            double sumX = 0, sumY = 0;
            int count = 0;

            for (const auto& point : points) {
                if (point.cluster == i) {
                    sumX += point.x;
                    sumY += point.y;
                    count++;
                }
            }

            if (count > 0) {
                newCentroids.append(DataPoint(sumX / count, sumY / count));
            } else {
                newCentroids.append(centroids[i]);
            }
        }

        bool converged = true;
        for (int i = 0; i < k; ++i) {
            if (calculateDistance(centroids[i], newCentroids[i]) > 0.001) {
                converged = false;
                break;
            }
        }

        centroids = newCentroids;
        if (converged) break;
    }

    QVector<QVector<DataPoint>> clusters(k);
    for (const auto& point : points) {
        if (point.cluster >= 0 && point.cluster < k) {
            clusters[point.cluster].append(point);
        }
    }

    return clusters;
}

int MainWindow::autoDetectClusters(const QVector<DataPoint>& points, int maxK)
{
    QVector<double> wcss;
    if (maxK == 15) maxK = qMin(99, points.size() / 3);

    for (int k = 1; k <= maxK && k < points.size(); ++k) {
        QVector<DataPoint> tempPoints = points;
        auto clusters = kMeansClustering(tempPoints, k);
        double totalWCSS = 0;
        for (int i = 0; i < clusters.size(); ++i) {
            double sumX = 0, sumY = 0;
            for (const auto& point : clusters[i]) {
                sumX += point.x;
                sumY += point.y;
            }
            if (clusters[i].size() > 0) {
                DataPoint centroid(sumX / clusters[i].size(), sumY / clusters[i].size());
                for (const auto& point : clusters[i]) {
                    totalWCSS += pow(calculateDistance(point, centroid), 2);
                }
            }
        }
        wcss.append(totalWCSS);
    }

    // NEW: More aggressive cluster detection
    int bestK = maxK / 2;  // Start with more clusters
    double maxDecrease = 0;

    // Look for the steepest drop in WCSS
    for (int i = 2; i < wcss.size() && i < 20; ++i) {  // Check up to 20 clusters
        double decrease = wcss[i-1] - wcss[i];
        double percentDecrease = decrease / wcss[i-1];

        // If decrease is less than 5%, we've found our elbow
        if (percentDecrease < 0.05 && i > 8) {  // Minimum 8 clusters
            bestK = i;
            break;
        }

        if (decrease > maxDecrease) {
            maxDecrease = decrease;
            bestK = i + 1;
        }
    }

    // Force minimum clusters based on data size
    int minClusters = qMax(8, points.size() / 50);  // At least 8, or 1 cluster per 50 points

    return qMax(minClusters, qMin(bestK, maxK));
}

void MainWindow::clusterAndRecolorBeamShifts(QCustomPlot* plot, const QList<QRgb>& colorScheme, int graphIndex) {
    if (!plot || plot->graphCount() <= graphIndex) {
        qDebug() << "ERROR: Invalid plot or graph index";
        return;
    }
    // Extract points
    QVector<DataPoint> points;
    auto dataContainer = plot->graph(graphIndex)->data();
    for (auto it = dataContainer->begin(); it != dataContainer->end(); ++it) {
        points.append(DataPoint(it->key, it->value));
    }
    qDebug() << "Extracted" << points.size() << "points from graph";
    if (points.size() < 2) {
        qDebug() << "ERROR: Not enough points";
        return;
    }
    // DISTANCE-BASED CLUSTERING with threshold
    QVector<QVector<DataPoint>> clusters;
    double threshold = findNaturalThreshold(points);
    qDebug() << "Using natural threshold:" << threshold;
    for (const auto& point : points) {
        bool addedToCluster = false;
        // Try to add to existing cluster
        for (auto& cluster : clusters) {
            if (!cluster.isEmpty()) {
                double dist = calculateDistance(point, cluster[0]);  // Distance to first point in cluster
                if (dist < threshold) {
                    cluster.append(point);
                    addedToCluster = true;
                    break;
                }
            }
        }
        // Create new cluster if point doesn't fit anywhere
        if (!addedToCluster) {
            QVector<DataPoint> newCluster;
            newCluster.append(point);
            clusters.append(newCluster);
        }
    }
    qDebug() << "Distance clustering found" << clusters.size() << "clusters";
    // Debug each cluster
    for (int i = 0; i < clusters.size(); ++i) {
        qDebug() << "Cluster" << i << "has" << clusters[i].size() << "points";
    }

    // Clear coordinate mapping
    coordinateToCluster.clear();

    // Plot each cluster
    plot->clearGraphs();
    qDebug() << "Cleared existing graphs";
    for (int i = 0; i < clusters.size(); ++i) {
        if (clusters[i].isEmpty()) continue;
        plot->addGraph();
        int colorIndex = (i * 15) % colorScheme.size();  // Skip colors for distinctness
        QColor color(colorScheme[colorIndex]);
        plot->graph(i)->setPen(QPen(color, 3));
        plot->graph(i)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCross, color, color, 10));
        plot->graph(i)->setLineStyle(QCPGraph::lsNone);
        // Calculate center and add points
        double sumX = 0, sumY = 0;
        for (const auto& point : clusters[i])
        {
            plot->graph(i)->addData(point.x, point.y);

            // Store coordinate to cluster mapping
            coordinateToCluster[QPair<double, double>(point.x, point.y)] = i + 1;

            sumX += point.x;
            sumY += point.y;
        }
        // Add number label at center
        double centerX = sumX / clusters[i].size();
        double centerY = sumY / clusters[i].size();
        double offsetX = 0.02;
        double offsetY = 0.02;
        QCPItemText *label = new QCPItemText(plot);
        label->setPositionAlignment(Qt::AlignCenter);
        label->position->setType(QCPItemPosition::ptPlotCoords);
        label->position->setCoords(centerX + offsetX, centerY + offsetY);  // Add offset
        label->setText(QString::number(i + 1));
        label->setFont(QFont("Arial", 8, QFont::Bold));  // Smaller font: 14 -> 10
        label->setPen(QPen(Qt::black));
        label->setBrush(QBrush(Qt::white));
        label->setPadding(QMargins(2, 2, 2, 2));  // Smaller padding: 4 -> 2
        qDebug() << "Creating graph" << i << "with color index" << colorIndex << "and" << clusters[i].size() << "points";
    }
    plot->rescaleAxes();
    plot->replot();
    qDebug() << "FINAL: Created" << clusters.size() << "colored groups on plot";
}

double MainWindow::findNaturalThreshold(const QVector<DataPoint>& points) {
    QVector<double> allDistances;

    // Calculate all pairwise distances
    for (int i = 0; i < points.size(); ++i) {
        for (int j = i + 1; j < points.size(); ++j) {
            double dist = calculateDistance(points[i], points[j]);
            if (dist > 0) allDistances.append(dist);
        }
    }

    std::sort(allDistances.begin(), allDistances.end());

    qDebug() << "Distance range:" << allDistances.first() << "to" << allDistances.last();

    // Find the biggest jump in distances - that's where clusters separate
    double maxJumpRatio = 0;
    double bestThreshold = 0;

    for (int i = 100; i < allDistances.size() - 100; ++i) {  // Skip extremes
        double currentDist = allDistances[i];
        double nextDist = allDistances[i + 1];

        if (currentDist > 0) {
            double jumpRatio = nextDist / currentDist;

            if (jumpRatio > maxJumpRatio) {
                maxJumpRatio = jumpRatio;
                bestThreshold = (currentDist + nextDist) / 2.0;
            }
        }
    }

    qDebug() << "Biggest distance jump ratio:" << maxJumpRatio << "at threshold:" << bestThreshold;
    return bestThreshold;
}

void MainWindow::on_pushButtonPrepareCryosparc_clicked()
{

}

