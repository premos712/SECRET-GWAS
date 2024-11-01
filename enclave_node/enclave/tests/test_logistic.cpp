#include <fstream>
#include <iostream>
#include <map>
#define BUFFER_LINES 10
#define OUTPUT_FILE "test_logistic.out"

#include "enclave_old.h"
#include <vector>
#include <cstring>
#include <chrono>

const vector<vector<string>> covFiles = {
    {"../../samples/1kg-logistic-regression/isFemale1.tsv",
     "../../samples/1kg-logistic-regression/isFemale2.tsv"}};
const vector<string> yFiles = {
    "../../samples/1kg-logistic-regression/PurpleHair1.tsv",
    "../../samples/1kg-logistic-regression/PurpleHair2.tsv"};
const vector<string> allelesFiles = {
    "../../samples/1kg-logistic-regression/alleles1.tsv",
    "../../samples/1kg-logistic-regression/alleles2.tsv"};
const vector<string> dpiNames = {"DPI1", "DPI2"};
const vector<int> dpi_size = {100, 150};

const vector<string> covNames = {"1", "isFemale"};

map<string, int> dpi_map;
map<string, int> cov_map;
// index -1 is reserved for intercept

void init() {
    for (int i = 0; i < dpiNames.size(); i++) {
        dpi_map.insert(make_pair(dpiNames[i], i));
    }
    int j = 0;
    for (int i = 0; i < covNames.size(); i++) {
        if (covNames[i] == "1") {
            cov_map.insert(make_pair("1", -1));
        } else {
            cov_map.insert(make_pair(covNames[i], j));
            j++;
        }
    }
}

void getdpilist(char dpilist[ENCLAVE_READ_BUFFER_SIZE]) {
    stringstream list_ss;
    for (size_t i = 0; i < dpiNames.size(); i++) {
        list_ss << dpiNames[i] << "\t";
    }
    strcpy(dpilist, list_ss.str().c_str());
}

void getcovlist(char covlist[ENCLAVE_READ_BUFFER_SIZE]) {
    stringstream ss;
    for (auto& cov : covNames) {
        ss << cov << "\t";
    }
    strcpy(covlist, ss.str().c_str());
}

void gety(bool* rt, const char dpi[MAX_DPINAME_LENGTH],
          char y[ENCLAVE_READ_BUFFER_SIZE]) {
    *rt = true;
    static vector<ifstream> y_fstreams;
    if (y_fstreams.empty()) {
        for (auto& y_file : yFiles) {
            y_fstreams.push_back(ifstream(y_file));
            if (!y_fstreams.back().is_open())
                throw ReadtsvERROR("fail to open file " + y_file);
        }
    }
    string dpi_name(dpi);
    int index = dpi_map[dpi_name];
    stringstream ss;
    ss << y_fstreams[index].rdbuf();
    strcpy(y, ss.str().c_str());
    return;
}

void getcov(bool* rt, const char dpi[MAX_DPINAME_LENGTH],
            const char cov_name[MAX_DPINAME_LENGTH],
            char cov[ENCLAVE_READ_BUFFER_SIZE]) {
    *rt = true;
    if (cov_name == "1") {
        strcpy(cov, "1");
        return;
    }
    static vector<vector<ifstream>> cov_streams;
    if (cov_streams.empty()) {
        for (auto& cov : covNames) {
            if (cov == "1") continue;
            cov_streams.push_back(vector<ifstream>());
            for (auto& cov_file : covFiles[cov_map[cov]]) {
                cov_streams[cov_map[cov]].push_back(ifstream(cov_file));
                if (!cov_streams[cov_map[cov]].back().is_open())
                    throw ReadtsvERROR("fail to open file " + cov_file);
            }
        }
    }
    string cov_str(cov_name);
    string dpi_str(dpi);
    int cov_index = cov_map[cov_str];
    int dpi_index = dpi_map[dpi_str];
    stringstream ss;
    ss << cov_streams[cov_index][dpi_index].rdbuf();
    strcpy(cov, ss.str().c_str());
}

bool getbatch(bool* rt, char batch[ENCLAVE_READ_BUFFER_SIZE]) {
    static vector<fstream> alleles_stream;
    if (alleles_stream.empty()) {
        for (auto& fname : allelesFiles) {
            alleles_stream.push_back(fstream(fname));
            if (!alleles_stream.back().is_open()) {
                throw ReadtsvERROR("fail to open file " + fname);
            }
            string first_line;
            getline(alleles_stream.back(), first_line);
            // throw away the header line
        }
    }
    int index = 0;
    if (alleles_stream[index].eof()) {
        strcpy(batch, EOFSeperator);
        *rt = true;
        return true;
    }
    stringstream buffer_ss;
    for (size_t i = 0; i < BUFFER_LINES; i++) {
        string line;
        if (!getline(alleles_stream[index], line)) break;
        buffer_ss << line << "\n";
    }
    if (buffer_ss.str() == "\n") {
        strcpy(batch, EOFSeperator);
        *rt = true;
        return true;
    }
    strcpy(batch, buffer_ss.str().c_str());
    *rt = true;
    return true;
}

void writebatch(Row_T type, char buffer[ENCLAVE_READ_BUFFER_SIZE]) {
    static ofstream result_f;
    if (!result_f.is_open()) {
        result_f.open(OUTPUT_FILE);
    }
    result_f << buffer;
    return;
}

int main() {
    try {
        init();
        // DEBUG:
        auto start = std::chrono::high_resolution_clock::now();
        log_regression();
        // DEBUG: total execution time
        auto stop = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
        cout << "Enclave time total: " << duration.count() << endl;
    } catch (ERROR_t& err) {
        cerr << err.msg << endl;
    }
}