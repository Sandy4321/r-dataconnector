/*
(c) Copyright 2015 Hewlett Packard Enterprise Development LP

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/


#include "delimitersplitproducer.h"
#include <iostream>
#include <stdexcept>
#include <boost/format.hpp>
#include <glog/logging.h>

#include "ddc/globals.h"

namespace ddc {
namespace splitproducer {

DelimiterSplitProducer::DelimiterSplitProducer()
    :  blockSize_(0),
       fileEnd_(0),
       offset_(0),
       offsetInBlock_(0),
       splitEnd_(0),
       splitsDiscarded_(0),
       splitsProduced_(0),
       delimiter_('\n'),
       configured_(false),
       skipRecord_(false),
       skipHeader_(false)
{
}

DelimiterSplitProducer::~DelimiterSplitProducer()
{

}



void DelimiterSplitProducer::configure(base::ConfigurationMap& conf)
{
    GET_PARAMETER(fileEnd_,uint64_t,"fileEnd");
    GET_PARAMETER(splitEnd_,uint64_t,"splitEnd");
    GET_PARAMETER(blockReader_,blockreader::IBlockReaderPtr,"blockReader");
    blockSize_ = blockReader_->blockSize();
    GET_PARAMETER(offset_,uint64_t,"splitStart");
    GET_PARAMETER(delimiter_,uint8_t,"delimiter");

    if(offset_ > 0) {
        if(block_) block_.reset(); //discard old block if it exists
        if(offset_ >= fileEnd_) {
            throw std::runtime_error("offset >= fileEnd");
        }
        uint64_t blockStart = (((int64_t)offset_ - (int64_t)blockSize_) > 0) ? (offset_ - blockSize_) : 0;
        uint64_t end = std::min(blockStart + blockSize_, fileEnd_);
        uint64_t numBytes = end - blockStart;

        if (numBytes > 0) {
            DLOG(INFO) << "prev, requesting block, offset: " << blockStart << " numBytes: " << numBytes;
            block_ = blockReader_->getBlock(blockStart, numBytes); //get previous block
            requestedBlocks_.push_back(std::make_pair(blockStart, numBytes));
        }
        offsetInBlock_ = (offset_ > numBytes) ? numBytes - 1 : offset_ - 1;
        offset_ -= 1;//backtrack 1 to detect if we're in the middle of a record
        skipRecord_ = true;
    }
    else {
        GET_PARAMETER(skipHeader_,bool,"skipHeader");
    }
    configured_ = true;

}

base::ConfigurationMap DelimiterSplitProducer::getDebugInfo() {
    base::ConfigurationMap conf;
    conf["split"] = splitCopy_;
    conf["block"] = block_;
    conf["requestedBlocks"] = requestedBlocks_;
    return conf;
}

bool DelimiterSplitProducer::hasNext()
{
    if(!configured_) {
        throw std::runtime_error("not configured");
    }
    return offset_ < splitEnd_;

}

SplitPtr DelimiterSplitProducer::next()
{
    if(!configured_) {
        throw std::runtime_error("not configured");
    }

    /*
     * If offset != 0 backtrack 1 and skip first record
     * If offset == 0 scan for newfiles until:
     *   1. end is reached
     *   2. if end is reached and line is incomplete until newline found
     *   3. If fileEnd is reached return what we have scanned so far
     */


    if(offset_ >= fileEnd_) {
        throw std::runtime_error("out of file bounds");
    }
    bool inLastRecord = false;
    /*
     * while we're working on this split
     * of we're scanning the last record that ends beyond this split
     */
    while((offset_ < splitEnd_) || (inLastRecord && offset_ < fileEnd_)) {
        //
        if(!block_) {
            if (stopDdc) {
                stopDdc = false;
                throw std::runtime_error("User cancelled operation.");
            }
            uint64_t end = std::min(offset_ + blockSize_, fileEnd_);
            uint64_t numBytes = end - offset_;
            if (numBytes > 0) {
                DLOG(INFO) << "requesting block, offset: " << offset_ << " numBytes: " << numBytes;
                LOG(INFO) <<  boost::format(" Completed %3.2f%%\r")  % (100 * (float)offset_/(float)fileEnd_);
                block_ = blockReader_->getBlock(offset_, numBytes);
                requestedBlocks_.push_back(std::make_pair(offset_, numBytes));
            }
            //DLOG(INFO) << "block read";
            offsetInBlock_ = 0;
        }

        bool splitEndFound = false;
        if(block_->buffer[offsetInBlock_] == delimiter_) {
            //we found a new line
            splitEndFound = true;
        }

        //push the char unless it's the delimiter
        if(!splitEndFound) split_.push_back(block_->buffer[offsetInBlock_]);

        // advance offsets
        offset_ += 1;
        offsetInBlock_ += 1;

        //if we've consumed the whole block ...
        if(offsetInBlock_ == blockSize_) {
            //DLOG(INFO) << "block consumed!";
            block_.reset();
        }

        if(splitEndFound) {
            if(inLastRecord) inLastRecord = false;
            if(skipRecord_) {
                //discard this record as another executor is processing it
                DLOG(INFO) << "skippping record: " << split_;
                skipRecord_ = false;
                splitsDiscarded_++;
                split_.clear();
                //continues below ...
            }
            else if(skipHeader_) {
                //discard header
                DLOG(INFO) << "skippping header: " << split_;
                skipHeader_ = false;
                splitsDiscarded_++;
                split_.clear();
                //continues below ...
            }
            else {
                 SplitPtr res = SplitPtr(new Split(boost::shared_ptr<std::string>(new std::string(split_))));
                 splitCopy_.clear();
                 splitCopy_ = std::string(split_);
                 DLOG_IF(INFO, splitsProduced_ < 10) << "returning split: " << split_;
                 splitsProduced_++;
                 split_.clear();
                 return res;
            }
        }


        if(offset_ == splitEnd_) {
            inLastRecord = true;
        }

    } // while((offset_ < splitEnd_) || (inLastRecord && offset_ < fileEnd_)) {
    if((split_.size() > 0) && !skipRecord_ && !skipHeader_) {
        SplitPtr res = SplitPtr(new Split(boost::shared_ptr<std::string>(new std::string(split_))));
        DLOG(INFO) << "returning record: " << split_;
        split_.clear();
        return res;
    }
    else{
        //TODO how to signal empty without an exception
        throw EmptySplitException("Split is empty.");
    }
}

} // namespace splitproducer
} // namespace ddc

