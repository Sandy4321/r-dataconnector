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


#ifndef DDC_HDFSUTILS_BASICURLDOWNLOADER_H
#define DDC_HDFSUTILS_BASICURLDOWNLOADER_H

#include "hdfsutils/iurldownloader.h"

namespace ddc{
namespace hdfsutils {


class BasicUrlDownloader : public IUrlDownloader{

public:
    BasicUrlDownloader();
    ~BasicUrlDownloader();

    int download(const std::string &url, BufferPtr buffer);
    int download(const std::string &url, const Speed &speed, BufferPtr buffer);
private:
    int downloadSpeedLimit(const std::string &url, long lowSpeedLimit, long lowSpeedTime, BufferPtr buffer);

    CURL *curl_;

}; //BasicUrlDownloader

} // namespace hdfsutils
} // namespace ddc

#endif // DDC_HDFSUTILS_BASICURLDOWNLOADER_H
