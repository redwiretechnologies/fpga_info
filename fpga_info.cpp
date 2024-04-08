#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string>
#include <iostream>
#include <vector>
#include <cstring>
#include <sstream>
#include <fstream>

#define BRAM_PBASE 0x9F000000
#define BRAM_SIZE  0x1000

#define FROM_FILE 0

struct header {
    std::string name;
    std::string carrier_rev;
    std::string som;
    std::string som_rev;
    std::string date;
};

struct repo {
    std::string name;
    std::string commit;
    std::string commit_date;
    std::string commit_message;
    std::string modification_message;
    std::string current_mods;
};

class log {
    public:
        struct header;
        std::vector<struct repo> repos;
};

std::vector<std::string> split_by_delimiter(char *s, const char* delim) {
    std::vector<std::string> splits;
    std::string temp_string;
    char *token;

    token = strtok(s, delim);

    while(token != NULL) {
        temp_string = token;
        splits.push_back(temp_string);
        token = strtok(NULL, delim);
    }

    return splits;
}

std::vector<std::string> split (const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss (s);
    std::string item;

    while (getline (ss, item, delim)) {
        result.push_back (item);
    }

    return result;
}

struct header parse_header(std::string s) {

    struct header head;
    std::vector<std::string> splits = split(s, '|');
    std::vector<std::string> splits2 = split(splits[0], '&');

    head.name        = splits2[0];
    head.carrier_rev = splits2[1];
    head.som         = splits2[2];
    head.som_rev     = splits2[3];
    head.date        = splits[1];

    return head;
}

struct repo parse_repo(std::string s) {
    struct repo r;
    std::vector<std::string> splits = split(s, '|');

    r.name = splits[0];
    r.commit = splits[1];
    r.commit_date = splits[2];
    r.commit_message = splits[3];
    r.modification_message = "";
    r.current_mods = "";
    if(splits.size() > 4) {
        int i=4;
        int k;
        while (i < splits.size() && splits[i] != "Mods:") {
            r.commit_message = r.commit_message+"\n      "+splits[i];
            i++;
        }
        if (i >= splits.size())
            return r;
        i++;
        k = i;
        while (splits[k] != "Current Modifications") {
            if (k > i) {
                r.modification_message = r.modification_message+"\n   "+splits[k];
            }
            else {
                r.modification_message = r.modification_message+splits[k];
            }
            k++;
        }
        for(int j=k+1; j<splits.size(); j++) {
            if (j > k+1) {
                if (splits[j][0] != '?')
                    splits[j] = " " + splits[j];
                r.current_mods = r.current_mods+"\n      "+splits[j];
            }
            else {
                if (splits[j][0] != '?')
                    splits[j] = " " + splits[j];
                r.current_mods = r.current_mods+splits[j];
            }
        }
    }
    return r;
}

void read_bram(char *st, bool print) {
    uint32_t *bram32_vptr;
    int fd;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) != -1) {

        bram32_vptr = (uint32_t *)mmap(NULL, BRAM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, BRAM_PBASE);
        for(int i=0; i<BRAM_SIZE/4; i++) {
            for (int j=0; j<4; j++) {
                st[i*4+j] = (bram32_vptr[i] >> (3-j)*8) & 0xFF;
                if (print) printf("%c\n", st[i*4+j]);
            }
        }
        close(fd);
    }
}

void print_header(struct header head) {
    std::cout << std::endl;
    std::cout << "Project Name:     " << head.name << std::endl;
    std::cout << "Carrier Revision: " << head.carrier_rev << std::endl;
    std::cout << "SOM:              " << head.som << std::endl;
    std::cout << "SOM Revision:     " << head.som_rev << std::endl << std::endl;
    std::cout << "Build Date:       " << head.date << std::endl;
}

void print_repo(struct repo r) {
    std::cout << r.name << std::endl;
    std::cout << "\n   Commit: " << r.commit << std::endl;
    std::cout <<   "   Date:   " << r.commit_date << std::endl;
    std::cout << "\n      " << r.commit_message << std::endl;
    if (r.modification_message != "") {
        std::cout << "\n   Modification Message\n\n      " << r.modification_message << std::endl;
    }
    if (r.current_mods != "") {
        std::cout << "\n   Current Modifications\n      " << r.current_mods << std::endl;
    }
}

std::vector<struct repo> parse_repos(std::vector<std::string> r_strings) {
    std::vector<struct repo> repos;

    for (int i=0; i<r_strings.size(); i++) {
        struct repo r = parse_repo(r_strings[i]);
        repos.push_back(r);
    }

    return repos;
}

void print_separator() {

    std::string sep = "------------------------------------------------------------------------------------------\n";

    std::cout << std::endl << sep << sep << std::endl;
}

void print_repos(std::vector<struct repo> repos) {
    for (int i=0; i<repos.size(); i++) {
        print_separator();
        print_repo(repos[i]);
    }
    printf("\n");
}

void print_bram(char *st) {
    for (int i=0; i<BRAM_SIZE; i++) {
        printf("%c", st[i]);
    }
    printf("\n");
}

int main_normal(int argc, char** argv) {
    char st[BRAM_SIZE];

    read_bram(st, false);
    if (argc > 1 && argv[1][0] == '-') {
        print_bram(st);
    }
    else {
        std::vector<std::string> splits = split_by_delimiter(st, "~");
        struct header head = parse_header(splits[0]);
        splits.erase(splits.begin());
        print_header(head);
        std::vector<struct repo> repos = parse_repos(splits);

        print_repos(repos);
    }

    return 0;
}

int main_from_file() {
    std::ifstream myfile("git_log_compressed.txt");
    std::string st;

    std::getline(myfile, st);
    char *cstr = new char[st.length() + 1];
    strcpy(cstr, st.c_str());
    std::vector<std::string> splits = split_by_delimiter(cstr, "~");
    struct header head = parse_header(splits[0]);
    splits.erase(splits.begin());
    print_header(head);
    std::vector<struct repo> repos = parse_repos(splits);

    print_repos(repos);

    return 0;
}

int main(int argc, char** argv)
{
    if (FROM_FILE == 1) {
        main_from_file();
    } else {
        main_normal(argc, argv);
    }

    return 0;
}
