#include "chunkscheduler.h"
 
namespace ddc {
namespace scheduler {

ChunkScheduler::ChunkScheduler()
    : chunkIndex_(0),
      configured_(false),
      planCreated_(false)
{
}

ChunkScheduler::~ChunkScheduler()
{
}

void ChunkScheduler::configure(base::ConfigurationMap &conf)
{
    workerMap_ = boost::any_cast<WorkerMap>(conf["workerMap"]);
    fileUrl_ = boost::any_cast<std::string>(conf["fileUrl"]);
    options_ = boost::any_cast<base::ConfigurationMap>(conf["options"]);
    std::string protocol = base::utils::getProtocol(fileUrl_);
    if (hdfsutils::isHdfs(protocol)) {
        hdfsBlockLocator_ = boost::any_cast<hdfsutils::HdfsBlockLocatorPtr>(conf["hdfsBlockLocator"]);
        base::ConfigurationMap hdfsConf;
        hdfsConfigurationFile_ = boost::any_cast<std::string>(conf["hdfsConfigurationFile"]);
        hdfsConf["hdfsConfigurationFile"] = hdfsConfigurationFile_;
        hdfsConf["filename"] = base::utils::stripProtocol(fileUrl_);
        hdfsBlockLocator_->configure(hdfsConf);
    }

    base::ConfigurationMap::iterator it = conf.find("fileType");
    if (it != conf.end()) {
        extension_ = boost::any_cast<std::string>(it->second);
    }
    else {
        extension_ = base::utils::getExtension(fileUrl_);
    }

    configured_ = true;
}

Plan ChunkScheduler::schedule() {
    Plan plan;
    if(!configured_) {
        throw std::runtime_error("need to configure first");
    }

    std::string protocol = base::utils::getProtocol(fileUrl_);

    std::string filename = base::utils::stripProtocol(fileUrl_);

//    std::vector<ddc::hdfsutils::HdfsBlock> hdfsBlocks;
//    if (hdfsutils::isHdfs(protocol)) {
//        hdfsBlocks = hdfsBlockLocator_->getHdfsBlocks(filename);
//    }
//    else {
//        // PASS
//    }

    base::ConfigurationMap conf;
    conf["hdfsBlockLocator"] = hdfsBlockLocator_;

    std::vector<Chunk> chunks;
    std::vector<base::ConfigurationMap> configurations;

    if (extension_ == "csv") {
        //
        // for CSV we split the file in as many chunks as workers
        //
        //uint64_t numWorkers = workerMap_.size();
        uint64_t numExecutors = 0;
        for (WorkerMap::iterator it = workerMap_.begin(); it != workerMap_.end(); ++it) {
            numExecutors += (it->second)->numExecutors();
        }

        uint64_t numPartitions = numExecutors;

#if 0
        for(uint64_t i = 0; i < numPartitions; ++i) {
            Chunk c;
            c.id = i;
            c.start = i * (status.length / numPartitions);
            c.end = (i + 1) * (status.length / numPartitions);
            assert(c.end <= status.length);
            chunks.push_back(c);
            base::ConfigurationMap conf;
            conf["chunkStart"] = c.start;
            conf["chunkEnd"] = c.end;
            conf["schema"] = boost::any_cast<std::string>(options_["schema"]);
            DLOG(INFO) << "Chunk: " << i <<
                          " start: " << c.start <<
                          " end: " << c.end <<
                          " schema: " << boost::any_cast<std::string>(options_["schema"]);
            configurations.push_back(conf);
        }
#else
        std::vector<std::string> files;
        if (hdfsutils::isHdfs(protocol)) {
            hdfsutils::HdfsGlobber globber;
            base::ConfigurationMap conf;
            conf["hdfsConfigurationFile"] = hdfsConfigurationFile_;
            globber.configure(conf);
            files = globber.glob(filename);
        }
        else {
            files = base::utils::globpp(filename);            
        }
        if (files.empty()) {
            throw std::runtime_error("List of files to read is empty. Make sure the files exist.");
        }
        divide(numPartitions, files, protocol, chunks, configurations);
#endif
    }
    else if (extension_ == "orc") {
        std::unique_ptr<orc::Reader> orcReader;
        orc::ReaderOptions opts;
        if (hdfsutils::isHdfs(protocol)) {
            hdfsutils::HdfsInputStream *p = new hdfsutils::HdfsInputStream(filename);
            base::ConfigurationMap hdfsconf;
            hdfsconf["hdfsConfigurationFile"] = hdfsConfigurationFile_;
            p->configure(hdfsconf);
            std::unique_ptr<orc::InputStream> inputStream(p);
            orcReader = orc::createReader(std::move(inputStream), opts);
        }
        else {
            orcReader = orc::createReader(orc::readLocalFile(filename), opts);
        }

        for (uint64_t i = 0; i < orcReader->getNumberOfStripes(); ++i) {
            // get stripe offsets
            std::unique_ptr<orc::StripeInformation> stripeInfo = orcReader->getStripe(i);
            uint64_t offset = stripeInfo->getOffset();
            uint64_t length = stripeInfo->getLength();
//            uint64_t indexLength = stripeInfo->getIndexLength();
//            uint64_t dataLength = stripeInfo->getDataLength();
//            uint64_t footerLength = stripeInfo->getFooterLength();
            Chunk c;
            c.id = i;
            c.start = offset;
            c.end = offset + length;
            c.protocol = protocol;
            c.filename = filename;
            chunks.push_back(c);

            base::ConfigurationMap conf;
            conf["selectedStripes"] = base::utils::to_string(i);
            conf["url"] = fileUrl_;

            configurations.push_back(conf);
        }

    }
    else {
        throw std::runtime_error("Unsupported file type or cannot determine extension");
    }

    SchedulerPtr s = SchedulerFactory::createScheduler(protocol, extension_);
    s->configure(conf);

    // ChunkMap:
    //   worker1 -> [chunk1, chunk2 ...]
    //   worker2 -> [chunk3, chunk4 ...]
    WorkerChunksMap _map;
    ChunkWorkerMap _map2;
    s->schedule(chunks, workerMap_, &_map, &_map2);

    workerChunksMap_ = _map;
    chunkWorkerMap_ = _map2;

    LOG(INFO) << "Worker -> Chunk map: " << workerChunksMap_;
    LOG(INFO) << "Chunk -> Worker map: " << chunkWorkerMap_;
    plan.numSplits = chunkWorkerMap_.size();
    plan.configurations = configurations;

    planCreated_ = true;

    return plan;
}

int32_t ChunkScheduler::getNextWorker() {
    if(!planCreated_) {
        throw std::runtime_error("Need to call schedule() first to create plan");
    }
    if (chunkWorkerMap_.find(chunkIndex_) == chunkWorkerMap_.end() ) {
        // not found
        std::ostringstream os;
        os << "chunkIndex " << chunkIndex_ << " doesn't exist in chunkWorkerMap";
        throw std::runtime_error(os.str());
    }
    int32_t worker = chunkWorkerMap_[chunkIndex_];
    ++chunkIndex_;
    return worker;
}

ChunkWorkerMap ChunkScheduler::chunkWorkerMap() const
{
    return chunkWorkerMap_;
}

WorkerChunksMap ChunkScheduler::workerChunksMap() const
{
    return workerChunksMap_;
}

struct Key {
    Key() : size(0) {

    }

    Key(const uint64_t _size,
        const Chunk _chunk) :
        size(_size),
        chunk(_chunk) {

    }

    bool operator==(const Key &other) const {
        return (size == other.size &&
                chunk == other.chunk);
    }

    uint64_t size;
    Chunk chunk;
};

struct KeyCompare
{
    // The map object uses this expression to determine both the order the elements
    // follow in the container and whether two element keys are equivalent
    // (by comparing them reflexively: they are equivalent if
    // !comp(a,b) && !comp(b,a)). No two elements in a map container can have equivalent keys.

   bool operator() (const Key& lhs, const Key& rhs) const {
       // first sort by size, biggest first
       // if sizes are equal sort by start, smallest first
       if (lhs.size != rhs.size) {
           return lhs.size > rhs.size;
       }
       else {
           if (lhs.chunk.start != rhs.chunk.start) {
               return lhs.chunk.start < rhs.chunk.start;
           }
           else {
                return lhs.chunk.filename < rhs.chunk.filename;
           }

       }
   }
};

struct KeyCompare2
{
   bool operator() (const Chunk& lhs, const Chunk& rhs) const {
       // sort by filename and then by chunkStart
       if (lhs.filename != rhs.filename) {
           return lhs.filename < rhs.filename;
       }
       else {
            return lhs.start < rhs.start;
       }
   }
};

void ChunkScheduler::divide(const uint64_t numExecutors,
                            const std::vector<std::string>& files,
                            const std::string& protocol,
                            std::vector<Chunk>& chunks,
                            std::vector<base::ConfigurationMap>& configurations) {

    if (files.empty()) {
        throw std::runtime_error("Divide called with an empty vector of files");
    }
    typedef std::map<Key, bool, KeyCompare> Map;
    Map map;
    for(uint64_t i = 0; i < files.size(); ++i) {
        base::IFilePtr file = hdfsutils::FileFactory::makeFile(protocol, files[i], "r");
        if (hdfsutils::isHdfs(protocol)) {
            hdfsutils::HdfsFile *p = (hdfsutils::HdfsFile *)file.get();
            base::ConfigurationMap conf;
            conf["hdfsConfigurationFile"] = hdfsConfigurationFile_;
            p->configure(conf);
        }
        uint64_t fileSize = (file->stat()).length;
        Key key(fileSize, Chunk(i, protocol, files[i], 0, fileSize));
        map[key] = true;
    }
    while (map.size() < numExecutors) {
        Map::iterator it = map.begin();
        Key biggest = it->first;
        uint64_t size = biggest.chunk.end - biggest.chunk.start;
        // split chunk in 2
        map.erase(biggest);
        Key smallKey1(size/2, Chunk(map.size(), protocol, biggest.chunk.filename, biggest.chunk.start, biggest.chunk.start + (size / 2)));
        Key smallKey2(size/2, Chunk(map.size() + 1, protocol, biggest.chunk.filename, biggest.chunk.start + (size / 2), biggest.chunk.start + size));
        map[smallKey1] = true;
        map[smallKey2] = true;
    }

    // reorder by chunkStart
    typedef std::map<Chunk, bool, KeyCompare2> Map2;
    Map2 map2;
    for(Map::iterator it = map.begin(); it != map.end(); ++it) {
        map2[(it->first).chunk] = true;
    }


    // copy to vector
    for(Map2::iterator it = map2.begin(); it != map2.end(); ++it) {
        std::string fullProtocol;
        if ((it->first).protocol == "") {
            fullProtocol = "";
        }
        else {
            fullProtocol = protocol + "://";
        }
        base::ConfigurationMap conf;
        conf["chunkStart"] = (it->first).start;
        conf["chunkEnd"] = (it->first).end;
        conf["schema"] = boost::any_cast<std::string>(options_["schema"]);
        conf["url"] = fullProtocol + (it->first).filename;
        configurations.push_back(conf);
        chunks.push_back(it->first);
    }
}

std::ostream &operator<<(std::ostream &stream, const Chunk& chunk) {
    stream << "id: " << chunk.id <<
              " start: " << chunk.start <<
              " end: " << chunk.end;
    return stream;
}

std::ostream &operator<<(std::ostream &stream, const WorkerChunksMap &map) {
    for(WorkerChunksMap::const_iterator it = map.begin(); it != map.end(); ++it) {
        stream << std::endl << "Worker " <<  it->first << ": " << std::endl;
        for (int i = 0; i < (it->second).size(); ++i) {
            stream << "    Chunk: " << (it->second)[i] << std::endl;
        }
    }
    return stream;
}

std::ostream &operator<<(std::ostream &stream, const ChunkWorkerMap &map) {
    for(ChunkWorkerMap::const_iterator it = map.begin(); it != map.end(); ++it) {
        stream << std::endl << "Chunk " <<  it->first << ": worker: " << it->second << std::endl;
    }
    return stream;
}

//void ChunkScheduler::setConfigurations(const std::vector<base::ConfigurationMap> &configurations)
//{
//    configurations_ = configurations;
//}


}  // namespace scheduler
}  // namespace ddc