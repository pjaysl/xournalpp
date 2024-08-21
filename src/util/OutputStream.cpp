#include "util/OutputStream.h"

#include <cstdio>
#include <fstream>
#include <cassert>
#include <cerrno>
#include <cstring>  // for strlen
#include <iostream>
#include <ostream>
#include <utility>  // for move

#include "util/GzUtil.h"  // for GzUtil
#include "util/i18n.h"    // for FS, _F
#include "util/safe_casts.h"

OutputStream::OutputStream() = default;

OutputStream::~OutputStream() = default;

void OutputStream::write(const std::string& str) { write(str.c_str(), str.length()); }

void OutputStream::write(const char* str) { write(str, std::strlen(str)); }

////////////////////////////////////////////////////////
/// GzOutputStream /////////////////////////////////////
////////////////////////////////////////////////////////

GzOutputStream::GzOutputStream(fs::path file): file(std::move(file)) {
    this->fp = GzUtil::openPath(this->file, "w");
    this->ofp = std::ofstream(this->file,std::ios::out | std::ios::trunc);
    // this->ofp.write("          ",10);
    this->ofp <<"                       "<<std::endl;
    // this->ofp.write("\n ",1);
    // this->ofp.write("\n ",1);
    // std::cout<<"-------"<<std::endl;
    // this->ofp.open(this->fp);
    if (this->fp == nullptr) {
        this->error = FS(_F("Error opening file: \"{1}\"") % this->file.u8string());
        this->error = this->error + "\n" + std::strerror(errno);
    }
}

GzOutputStream::~GzOutputStream() {
    if (this->fp) {
        close();
    }
    this->fp = nullptr;
}

auto GzOutputStream::getLastError() const -> const std::string& { return this->error; }

void GzOutputStream::write(const char* data, size_t len) {
    xoj_assert(len != 0 && this->fp);
    // std::ofstream fil(this->file,std::ios::binary);
    // std::cout<<len<<std::endl<<data<<std::endl;
    // printf("%s",data);
    // std::cout<<"------------"<<std::endl;
    // std::cout<<data<<std::endl;
    // std::string str(data);
    // std::cout<<str<<std::endl;
    // std::cout<<"--"<<len<<"--";
    this->ofp.write(data,long(len));
    // this->ofp << str;
    // this->ofp.write(data,long(len));
    // auto written = gzwrite(this->fp, data, strict_cast<unsigned int>(len));
    // if (as_unsigned(written) != len) {
    //     int errnum = 0;
    //     const char* error = gzerror(this->fp, &errnum);
    //     if (errnum != Z_OK) {
    //         this->error = FS(_F("Error writing data to file: \"{1}\"") % this->file.u8string());
    //         this->error += "\n" + FS(_F("Error code {1}. Message:") % errnum) + "\n";
    //         if (errnum == Z_ERRNO) {
    //             // fs error. Fetch the precise message
    //             this->error += std::strerror(errno);
    //         } else {
    //             this->error += error;
    //         }
    //     }
    // }
}

void GzOutputStream::close() {
    this->ofp.close();
    this->removeFirstLine(this->file);
    this->removePreviewLine(this->file);
    if (!this->fp) {
        return;
    }

    auto errnum = gzclose(this->fp);
    this->fp = nullptr;

    if (errnum != Z_OK) {
        this->error = FS(_F("Error occurred while closing file: \"{1}\"") % this->file.u8string());
        this->error += "\n" + FS(_F("Error code {1}") % errnum);
        if (errnum == Z_ERRNO) {
            // fs error. Fetch the precise message
            this->error = this->error + "\n" + std::strerror(errno);
        }
    }
}


void GzOutputStream::removeFirstLine(const fs::path& filePath) {
    // Temporary file path
    fs::path tempFilePath = filePath;
    tempFilePath += ".tmp";

    // Open the original file for reading
    std::ifstream inFile(filePath);
    if (!inFile) {
        std::cerr << "Failed to open file for reading: " << filePath << std::endl;
        return;
    }

    // Open the temporary file for writing
    std::ofstream outFile(tempFilePath);
    if (!outFile) {
        std::cerr << "Failed to open temporary file for writing: " << tempFilePath << std::endl;
        inFile.close();
        return;
    }

    // Read the first line and discard it
    std::string firstLine;
    std::getline(inFile, firstLine);

    // Copy the rest of the file to the temporary file
    outFile << inFile.rdbuf();

    // Close both files
    inFile.close();
    outFile.close();

    // Remove the original file
    if (std::remove(filePath.c_str()) != 0) {
        std::cerr << "Failed to remove original file: " << filePath << std::endl;
        return;
    }

    // Rename the temporary file to the original file name
    if (std::rename(tempFilePath.c_str(), filePath.c_str()) != 0) {
        std::cerr << "Failed to rename temporary file to original file name: " << tempFilePath << std::endl;
    }
}

void GzOutputStream::removePreviewLine(const std::string& filePath) {
    // Temporary file path
    const std::string tempFilePath = filePath + ".tmp";

    // Open the original file for reading
    std::ifstream inFile(filePath);
    if (!inFile) {
        std::cerr << "Failed to open file for reading: " << filePath << std::endl;
        return;
    }

    // Open the temporary file for writing
    std::ofstream outFile(tempFilePath);
    if (!outFile) {
        std::cerr << "Failed to open temporary file for writing: " << tempFilePath << std::endl;
        inFile.close();
        return;
    }

    // Read the file line by line
    std::string line;
    bool previewLineFound = false;
    while (std::getline(inFile, line)) {
        if (line.find("<preview>") == 0 && line.find("</preview>") == line.length() - 10) {
            previewLineFound = true;
            continue; // Skip the line that matches the pattern
        }
        outFile << line << std::endl;
    }

    // Close both files
    inFile.close();
    outFile.close();

    // If a preview line was found, proceed with file replacement
    if (previewLineFound) {
        // Remove the original file
        if (std::remove(filePath.c_str()) != 0) {
            std::cerr << "Failed to remove original file: " << filePath << std::endl;
            return;
        }

        // Rename the temporary file to the original file name
        if (std::rename(tempFilePath.c_str(), filePath.c_str()) != 0) {
            std::cerr << "Failed to rename temporary file to original file name: " << tempFilePath << std::endl;
        }
    } else {
        // If no preview line was found, remove the temporary file
        std::remove(tempFilePath.c_str());
    }
}
