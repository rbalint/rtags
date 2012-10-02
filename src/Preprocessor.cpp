#include "Preprocessor.h"
#include "Connection.h"
#include "Process.h"
#include "Log.h"

Preprocessor::Preprocessor(const Path& filename, const List<ByteArray>& arguments, Connection* connection)
    : mFilename(filename), mArguments(arguments), mConnection(connection), mProc(0)
{
}

Preprocessor::~Preprocessor()
{
    delete mProc;
}

void Preprocessor::preprocess()
{
    if (!mProc) {
        mProc = new Process;
        mProc->readyReadStdOut().connect(this, &Preprocessor::onProcessReadyRead);
        mProc->finished().connect(this, &Preprocessor::onProcessFinished);
    }
    mArguments.append("-E");
    mArguments.append(mFilename);
    mProc->start("g++", mArguments);
}

void Preprocessor::onProcessReadyRead()
{
    mConnection->write(mProc->readAllStdOut());
}

void Preprocessor::onProcessFinished()
{
    mConnection->finish();
    deleteLater();
}
