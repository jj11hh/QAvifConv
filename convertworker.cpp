#include "convertworker.h"
#include "jpegavifconverter.h"
#include <QDirIterator>
#include <QDebug>
#include <QThread>
#include <QTimer>

void ConvertWorker::setPath(const QString &src, const QString &dst){
    mutex.lock();
    this->srcPath = src;
    this->dstPath = dst;
    mutex.unlock();
}

void ConvertWorker::setParameter(const ConvertSettings & param){
    mutex.lock();
    settings = param;
    mutex.unlock();
}

void ConvertWorker::setAction(WorkerAction _action){
    mutex.lock();
    action = _action;
    mutex.unlock();
}

void ConvertWorker::abort(){
    qDebug() << "user issued abort";
    mutex.lock();
    flagAbort = true;
    mutex.unlock();
}

void ConvertWorker::doWork()
{
    QDir src;
    QDir dst;
    WorkerAction doAction;
    QStringList fileFilter;

    mutex.lock();
    src.setPath(srcPath);
    dst.setPath(dstPath);
    doAction = action;
    mutex.unlock();


    file_count = 0;
    file_passed = 0;
    files.clear();

    if (doAction == ConvertWorker::JpegToAvif)
        fileFilter << "*.jpeg" << "*.jpg";
    else if (doAction == ConvertWorker::AvifToJpeg)
        fileFilter << "*.avif";


    auto iter = QDirIterator(
        src.absolutePath(),
        fileFilter,
        QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Files | QDir::Readable,
        QDirIterator::Subdirectories
    );

    while (iter.hasNext()){
        iter.next();
        file_count ++;
        files.append(iter.filePath());
    }
    QTimer::singleShot(0, this, SLOT(processImage()));
}

void ConvertWorker::processImage(){
    QString relativePath;
    QString dstPathString;
    QString dstDir;
    JpegAvifConverter convertor(settings);
    QDir src, dst;
    WorkerAction doAction;


    if (file_passed >= files.length()){
        emit workDone();
        return;
    }

    mutex.lock();
    if (flagAbort){
        qDebug() << "task abort";
        mutex.unlock();
        emit workDone();
        return;
    }
    src.setPath(srcPath);
    dst.setPath(dstPath);
    doAction = action;
    mutex.unlock();

    auto file = files[file_passed];
    relativePath = src.relativeFilePath(file);
    dstPathString = dst.filePath(relativePath);

    int i;
    for (i = dstPathString.length() - 1; dstPathString.at(i) != '/'; i --);
    //                                                           ^
    //                                                           | it works, because Qt will convert for us
    dstDir = dstPathString.chopped(dstPathString.length() - i - 1);

    QDir().mkpath(dstDir);

    for (i = dstPathString.length() - 1; dstPathString.at(i) != '.'; i --);
    dstPathString.chop(dstPathString.length() - i - 1);

    switch (doAction){
    case ConvertWorker::JpegToAvif:
        dstPathString.append("avif");
        break;
    case ConvertWorker::AvifToJpeg:
        dstPathString.append("jpg");
        break;
    }

    bool result = false;
    emit resultReady(tr("正在转换") + dstPathString);
    qDebug() << "converting " << file;
    qDebug() << "to " << dstPathString;

    if (QFileInfo::exists(dstPathString)){
        emit resultReady(tr("文件已存在，跳过"));
    }
    else {
        if (doAction == ConvertWorker::JpegToAvif)
            result = convertor.ConvertJpegToAvif(file, dstPathString);
        else if (doAction == ConvertWorker::AvifToJpeg)
            result = convertor.ConvertAvifToJpeg(file, dstPathString);

        emit resultReady(result ? tr("成功") : tr("失败"));
    }
    file_passed ++;
    emit progressReady(file_passed * 100 / file_count);

    QTimer::singleShot(0, this, SLOT(processImage()));
}

