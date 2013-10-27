#include "IndexerJob.h"
#include "Project.h"
#include <rct/Process.h>
#include <RTagsClang.h>
#include "Server.h"

IndexerJob::IndexerJob(IndexType t, const Path &p, const Source &s)
    : state(Pending), destination(Server::instance()->options().socketFile),
      port(0), type(t), project(p), source(s), sourceFile(s.sourceFile()), process(0)
{
}

IndexerJob::IndexerJob()
    : state(Pending), port(0), type(Invalid), process(0)
{
}

void IndexerJob::preprocess()
{
    if (preprocessed.isEmpty())
        preprocessed = RTags::preprocess(source);
}

bool IndexerJob::startLocal()
{
    assert(state == Pending);
    state = Running;
    assert(!process);
    preprocess();
    static const Path rp = Rct::executablePath().parentDir() + "rp";
    String stdinData;
    Serializer serializer(stdinData);
    encode(serializer);

    process = new Process;
    if (!port)
        process->finished().connect(std::bind(&IndexerJob::onProcessFinished, this));
    if (!process->start(rp)) {
        error() << "Couldn't start rp" << process->errorString();
        delete process;
        process = 0;
        return false;
    }
    process->write(stdinData);
    return true;
}

bool IndexerJob::update(IndexType t, const Source &s)
{
    switch (state) {
    case Aborted:
        assert(0);
        break;
    case Running:
        abort();
        break;
    case Pending:
        type = t;
        source = s;
        return true;
    }
    return false;
}

void IndexerJob::abort()
{
    switch (state) {
    case Aborted:
    case Pending:
        break;
    case Running:
        if (process) {
            process->kill();
            delete process;
            process = 0;
            // ### probably need to disconnect signals
        }
        break;
    }
    state = Aborted;
}

void IndexerJob::encode(Serializer &serializer)
{
    serializer << destination << port << sourceFile
               << source << preprocessed << project << static_cast<uint8_t>(type)
               << Server::instance()->options().rpVisitFileTimeout
               << Server::instance()->options().rpIndexerMessageTimeout;
}

void IndexerJob::decode(Deserializer &deserializer)
{
    uint8_t t;
    int ignored; // timeouts
    deserializer >> destination >> port >> sourceFile
                 >> source >> preprocessed >> project >> t
                 >> ignored >> ignored;
    type = static_cast<IndexType>(t);
}

void IndexerJob::onProcessFinished()
{
    assert(!port);
    // error() << "PROCESS FINISHED" << source.sourceFile() << process->returnCode() << this;
    ::error() << process->readAllStdOut();
    ::error() << process->readAllStdErr();
    if (process->returnCode() == -1) {
        std::shared_ptr<Project> proj = Server::instance()->project(project);
        if (proj || proj->state() != Project::Loaded) {
            std::shared_ptr<IndexData> data(new IndexData(type));
            data->fileId = source.fileId;
            data->aborted = true;
            EventLoop::SharedPtr loop = EventLoop::eventLoop();
            assert(loop);
            loop->callLater([proj, data]() { proj->onJobFinished(data); });
        }
    }
    // ::error() << source.sourceFile() << "finished" << process->returnCode() << mWaiting << mTimer.elapsed() << "ms";
}
