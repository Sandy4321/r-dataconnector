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


#ifndef DDC_BLOCKREADER_LOCALBLOCKREADER_H
#define DDC_BLOCKREADER_LOCALBLOCKREADER_H

#include <stdio.h>
#include <string>
#include <vector>
#include "base/scopedfile.h"
#include "blockreader/iblockreader.h"

namespace ddc{
namespace blockreader {


class LocalBlockReader : public IBlockReader
{
public:
    LocalBlockReader();
    ~LocalBlockReader();

    void configure(base::ConfigurationMap &conf);

    boost::shared_ptr<Block> next();
    bool hasNext();

    uint64_t blockSize();

    BlockPtr getBlock(const uint64_t blockStart, const uint64_t numBytes);


private:
    uint64_t blockSize_;
    std::string filename_;
    base::ScopedFilePtr f_;
    bool configured_;

    uint64_t fileSize_;
};

} // namespace blockreader
} // namespace ddc

#endif // DDC_BLOCKREADER_LOCALBLOCKREADER_H
