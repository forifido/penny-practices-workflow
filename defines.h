//
// Created by k on 22-4-29.
//

#include <iostream>
#include <vector>

#ifndef BIDU_HOTNEWS_BAOBIAN_SERVER_COMMON_H
#define BIDU_HOTNEWS_BAOBIAN_SERVER_COMMON_H

struct _item {
    std::string url;
    std::string title;
    float idx;
};

struct merged_result {
    std::mutex _mutex;
    std::vector<_item> items;
};

const std::string BIDU_NEWS_URL = "http://news.baidu.com";
const std::string BAOBIAN_API = "http://baobianapi.pullword.com:9091/get.php";

#define REDIRECT_MAX    5
#define RETRY_MAX       2
#endif //BIDU_HOTNEWS_BAOBIAN_SERVER_COMMON_H
