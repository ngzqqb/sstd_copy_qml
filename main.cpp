

//一个简单的文件拷贝器
//假设只有文件和文件夹，不考虑链接等情况
//对.qml文件进行特殊处理
//将：
    /*begin:debug*/
    /*end:debug*/
//替换为：
    /*remove debug information*/
//将
    /*begin:import*/
    /*end:import*/
//之间的_the_debug删除

#include <list>
#include <set>
#include <array>
#include <regex>
#include <string>
#include <string_view>
#include <cassert>
using namespace std::string_literals;
using namespace std::string_view_literals;

#include <utility>
#include <algorithm>

enum ErrorCodeC : int {
    ArgNotEnough = -99,
    CopyDirError,
    CopyFileError,
    UnknowError,
};

#if __has_include( <filesystem> )
#include <filesystem>
namespace the {
    namespace filesystem = std::filesystem;
    const auto getStreamFileName(const filesystem::path & arg) {
        return arg;
    }
}/**/
#else
#include <experimental/filesystem>
namespace the {
    namespace filesystem = std::experimental::filesystem;
    auto getStreamFileName(const filesystem::path & arg) {
        return arg.string();
    }
}/**/
#endif

#include <fstream>
#include <iostream>
#include <cctype>

namespace the {

    inline std::set<std::string, std::less<>> & configs() {
        static std::set<std::string, std::less<>> varAns;
        return varAns;
    }

    inline bool deleteDir(const filesystem::path & arg) try {
        return filesystem::remove_all(arg);
    } catch (...) {
        return false;
    }

    inline bool deleteFile(const filesystem::path & arg) try {
        return filesystem::remove(arg);
    } catch (...) {
        return false;
    }

    inline std::string trime_left(std::string varArg) {
        std::size_t varFirstNotSpace{ 0 };
        for (const auto & varI : varArg) {
            if (::isspace(varI)) {
                ++varFirstNotSpace;
            } else {
                break;
            }
        }
        auto varLength = varArg.size() - varFirstNotSpace;
        if (varLength < 1) {
            return ""s;
        }
        return { varArg.data() + varFirstNotSpace, varLength };
    }

    inline void remove_utf8_bom(std::ifstream & argReadStream) {
        std::array varBuffer{ '1','2','3' };
        argReadStream.read(varBuffer.data(), varBuffer.size());
        if (argReadStream.gcount() < 3) {
        } else {
            constexpr std::array varBom{ '\xEF','\xBB','\xBF' };
            if (varBuffer == varBom) {
                return;
            }
        }
        argReadStream.clear();
        argReadStream.seekg(0);
        assert(argReadStream.good());
        return;
    }

    template<typename T, typename U>
    inline void __parser_qmldir(T &varReadStream, U&varOutStream) try {

        /*qmldir 不需要bom*/
        remove_utf8_bom(varReadStream);

        std::string varDebugModuleName;
        std::string varReleaseModuleName;

        constexpr auto varModule = "module"sv;

        using Line = std::string;

        std::vector<Line> varLines;
        const static std::regex varRegexTheDebug{ u8R"(_the_debug)", std::regex::icase };

        while (varReadStream.good()) {
            Line varLine;
            std::getline(varReadStream, varLine);
            varLine = trime_left(varLine);
            if ((varLine.find(varModule) == 0) && (varLine.size() > varModule.size()) &&
                (false != ::isspace(varLine[varModule.size()]))) {
                varDebugModuleName = trime_left(Line(varLine.data() + varModule.size(),
                    varLine.size() - varModule.size()));
                if (varReleaseModuleName.empty()) {
                    varReleaseModuleName = std::regex_replace(varDebugModuleName, varRegexTheDebug, ""s);
                }
                continue;
            }
            varLines.push_back(std::move(varLine));
        }

        if (configs().count("release") > 0) {
            if (varReleaseModuleName.size()) {
                varOutStream << "module "sv << varReleaseModuleName << '\n';
            }
        } else {
            if (varDebugModuleName.size()) {
                varOutStream << "module "sv << varDebugModuleName << '\n';
            }
        }

        for (const auto & varI : varLines) {
            varOutStream << varI << '\n';
        }

    } catch (...) {
    }

    inline bool isSame(const filesystem::path & varFrom, const filesystem::path & varTo) {
        std::ifstream varRead1{ getStreamFileName(varFrom),std::ios::binary };
        std::ifstream varRead2{ getStreamFileName(varTo),std::ios::binary };

        if (varRead1.is_open() == false) {
            return false;
        }

        if (varRead2.is_open() == false) {
            return false;
        }

        alignas(32) char varBuffer1[1024 * 4];
        alignas(32) char varBuffer2[1024 * 4];

        do {
            varRead1.read(varBuffer1, sizeof(varBuffer1));
            auto varValue1 = varRead1.gcount();
            varRead2.read(varBuffer2, sizeof(varBuffer2));
            auto varValue2 = varRead2.gcount();
            if (varValue1 != varValue2) {
                return false;
            }
            if ((varValue1 > 0) &&
                (0 != memcmp(varBuffer1, varBuffer2, static_cast<std::size_t>(varValue1)))) {
                return false;
            }
        } while (varRead1.good() && varRead2.good());

        return varRead1.good() == varRead2.good();

    }

    template<typename T, typename U>
    inline void __parser_qml(T &varReadStream, U&varOutStream) try {

        enum {
            normal_line = 0,
            begin_type = 1,
            end_type = 2,
            begin_import_type,
            end_import_type
        };

        class Line : public std::string {
        public:
            std::size_t type = normal_line;
        };

        std::vector<Line> varLines;

        const static std::regex varRegexDebugBegin{ "(?:" "\xef" "\xbb" "\xbf" ")?" "\\s*/\\*begin:debug\\*/\\s*" , std::regex::icase };
        const static std::regex varRegexDebugEnd{ u8R"(\s*/\*end:debug\*/\s*)", std::regex::icase };
        const static std::regex varRegexImportBegin{ "(?:" "\xef" "\xbb" "\xbf" ")?" "\\s*/\\*begin:import\\*/\\s*" , std::regex::icase };
        const static std::regex varRegexImportEnd{ u8R"(\s*/\*end:import\*/\s*)", std::regex::icase };
        bool hasDebugData = false;

        while (varReadStream.good()) {
            Line varLine;
            std::getline(varReadStream, varLine);
            if (varLine.empty() == false) {
                if (std::regex_match(varLine, varRegexDebugBegin)) {
                    varLine.type = begin_type;
                    hasDebugData = true;
                } else if (std::regex_match(varLine, varRegexDebugEnd)) {
                    varLine.type = end_type;
                    hasDebugData = true;
                } else if (std::regex_match(varLine, varRegexImportEnd)) {
                    varLine.type = end_import_type;
                    hasDebugData = true;
                } else if (std::regex_match(varLine, varRegexImportBegin)) {
                    varLine.type = begin_import_type;
                    hasDebugData = true;
                }
            }
            varLines.push_back(std::move(varLine));
        }

        if (varLines.empty()) {
            return;
        }

        if (false == hasDebugData) {
            /*简单复制*/
            for (const auto & varI : varLines) {
                varOutStream << varI << '\n';
            }
            return;
        }

        int varDebugCount = 0;
        int varImportCount = 0;
        const auto varIsRelease = configs().count("release"sv) > 0;
        const static std::regex varRegexTheDebug{ u8R"(_the_debug)", std::regex::icase };

        for (const auto & varLine : varLines) {

            const auto varOldDebugCount = varDebugCount;

            switch (varLine.type) {
            case begin_type:++varDebugCount; break;
            case end_type:--varDebugCount; break;
            case begin_import_type:++varImportCount; break;
            case end_import_type:--varImportCount; break;
            }

            if ((0 < varDebugCount) || (0 < varOldDebugCount)) {
                varOutStream << u8"/*remove debug information*/"sv << '\n';
            } else if (varIsRelease && (varImportCount > 0)) {
                varOutStream << std::regex_replace(varLine, varRegexTheDebug, ""s) << '\n';
            } else {
                varOutStream << varLine << '\n';
            }

        }

    } catch (...) {
    }

    inline bool copyAFile(
        const filesystem::path & varFrom,
        const filesystem::path & varTo) try {

        if (!filesystem::exists(varFrom)) {
            return false;
        }

        if (filesystem::is_directory(varFrom)) {
            return false;
        }

        {
            /*创建根路径*/
            auto varDir = varTo.parent_path();
            filesystem::create_directories(varDir);
        }

        if (filesystem::exists(varTo)) {

            if (filesystem::is_directory(varTo)) {/*is dir*/

                if (false == deleteDir(varTo)) {
                    return false;
                }

            } else {/*is file*/

                auto varReallyTo = filesystem::canonical(varTo);
                if (varReallyTo.filename() != varTo.filename()) {
                    filesystem::rename(varReallyTo, varTo);
                }

            }

        }

        class ReadWrite {
        public:
            std::ifstream read;
            std::ofstream write;
            inline ReadWrite(const filesystem::path & varFrom,
                const filesystem::path & varTo) : read{ getStreamFileName(varFrom) , std::ios::binary },
                write{ getStreamFileName(varTo)  , std::ios::binary }{
                read.sync_with_stdio(false);
                write.sync_with_stdio(false);
                if (false == read.is_open()) {
                    throw false;
                }
                if (false == write.is_open()) {
                    throw false;
                }
            }
        };

        if (varTo.filename().wstring() == LR"(qmldir)"sv) {
            ReadWrite var{ varFrom,varTo };
            __parser_qmldir(var.read, var.write);
            return true;
        }

        bool varIsQml;
        if (configs().count("debug"sv) > 0) {
            /*debug 模式直接copy*/
            varIsQml = false;
        } else {
            const static std::regex varRegex{ "\\.qml", std::regex::icase };
            const auto varExtension = varTo.extension().string();
            varIsQml = std::regex_match(varExtension, varRegex);
        }

        if (varIsQml) {
            ReadWrite var{ varFrom,varTo };
            __parser_qml(var.read, var.write);
        } else {
            if (filesystem::exists(varTo)) {
                if (isSame(varFrom, varTo)) {
                    return true;
                }
                ReadWrite var{ varFrom,varTo };
                var.write << var.read.rdbuf();
            } else {
                ReadWrite var{ varFrom,varTo };
                var.write << var.read.rdbuf();
            }
        }

        return true;
    } catch (...) {
        return false;
    }

    inline std::list< std::pair<filesystem::path, filesystem::path> >  _copyADir(
        const filesystem::path & varFrom, const filesystem::path & varTo) {

        if (!filesystem::exists(varFrom)) {
            return {};
        }

        if (!filesystem::is_directory(varFrom)) {
            return {};
        }

        {
            /*创建根路径*/
            auto varDir = varTo.parent_path();
            filesystem::create_directories(varDir);
        }

        if (filesystem::exists(varTo)) {

            if (!filesystem::is_directory(varTo)) {
                if (false == deleteFile(varTo)) {
                    return {};
                }
            }

            auto varReallyTo = filesystem::canonical(varTo);
            if (varReallyTo.filename() != varTo.filename()) {
                filesystem::rename(varReallyTo, varTo);
            }

        }

        filesystem::directory_iterator varPos{ varFrom };
        filesystem::directory_iterator varEnd;
        std::list< std::pair<filesystem::path, filesystem::path> > varAns;

        for (; varPos != varEnd; ++varPos) {

            if (filesystem::is_directory(varPos->status())) {

                filesystem::create_directories(varTo / varPos->path().filename());
                varAns.emplace_back(varPos->path(), varTo / varPos->path().filename());

            } else {

                if (false == copyAFile(varPos->path(), varTo / varPos->path().filename())) {
                    return {};
                }

            }

        }

        return std::move(varAns);

    }

    inline bool copyADir(
        const filesystem::path & varFrom, const filesystem::path & varTo) try {


        std::list< std::pair<filesystem::path, filesystem::path> > varDuty;
        varDuty.emplace_back(varFrom, varTo);

        while (!varDuty.empty()) {

            auto varItem = varDuty.front();
            varDuty.pop_front();

            varDuty.splice(varDuty.begin(),
                _copyADir(varItem.first, varItem.second));

        }

        return true;

    } catch (...) {
        return false;
    }

}/*namespace the*/


/*
0:the application
1:from file or dir
2:to file or dir
3...:options
*/
int main(int argc, char ** argv) try {

    static_assert(UnknowError < 0);

    if (argc < 3) {
        return ArgNotEnough;
    }

    for (int varIndex = 3; varIndex < argc; ++varIndex) {
        the::configs().insert(argv[varIndex]);
    }

    the::filesystem::path varFrom(argv[1]);
    the::filesystem::path varTo(argv[2]);

    if (the::filesystem::is_directory(varFrom)) {
        if (false == the::copyADir(varFrom, varTo)) {
            return CopyDirError;
        }
    } else {
        if (false == the::copyAFile(varFrom, varTo)) {
            return CopyFileError;
        }
    }

    return 0;

} catch (...) {
    return UnknowError;
}
