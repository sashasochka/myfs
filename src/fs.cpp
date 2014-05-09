#include "fs.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace std;

namespace myfs {

struct Link {
    int descr_block_id; // relative to the last bitmask block
    char filename[FILENAME_MAX_LENGTH + 1];
};

// INTERNAL LINKAGE SECTION
namespace {

struct Descriptor final {
    FileType type;
    int n_links;
    int size;
    int data_block_ids[BLOCKS_PER_DESCRIPTOR];
};

static_assert(sizeof(Descriptor) != BLOCK_SIZE, "Descriptor size != BLOCK_SIZE");
constexpr int ZERO_BLOCK = -1;
constexpr int BAD_BLOCK = -2;

auto device_capacity = -1;
auto n_bitmask_blocks = -1;
auto n_data_blocks = -1;
fstream fio;
Link root_link;

bool is_mounted() {
    return device_capacity != -1 && fio.is_open();
}

void read_block(int block_id, char* data, int size = BLOCK_SIZE, int shift = 0) {
    assert(is_mounted());
    assert(0 <= size);
    assert(0 <= shift);
    assert(size + shift <= BLOCK_SIZE);
    fio.seekg((block_id + n_bitmask_blocks) * BLOCK_SIZE + shift, fio.beg);
    fio.get(data, size);
#ifndef NDEBUG
    fio.flush();
#endif
}

void write_block(int block_id, const char* data, int size = BLOCK_SIZE, int shift = 0) {
    assert(is_mounted());
    assert(0 <= size);
    assert(0 <= shift);
    assert(size + shift <= BLOCK_SIZE);
    fio.seekp((block_id + n_bitmask_blocks) * BLOCK_SIZE + shift, fio.beg);
    fio.write(data, size);
#ifndef NDEBUG
    fio.flush();
#endif
}

void block_mark_used(int block_id) {
    assert(is_mounted());
    fio.seekg(block_id / 8, fio.beg);
    int old_mask = fio.get();
    assert(old_mask >= 0);
    fio.seekp(block_id / 8, fio.beg);
    fio.put(static_cast<char>(old_mask | (1 << (block_id % 8))));
#ifndef NDEBUG
    fio.flush();
#endif
}

void block_mark_unused(int block_id) {
    assert(is_mounted());
    fio.seekg(block_id / 8, fio.beg);
    int old_mask = fio.get();
    assert(old_mask >= 0);
    fio.seekp(block_id / 8, fio.beg);
    fio.put(static_cast<char>(old_mask & ~(1 << (block_id % 8))));
#ifndef NDEBUG
    fio.flush();
#endif
}

bool block_used(int block_id) {
    assert(is_mounted());
    fio.seekg(block_id / 8, fio.beg);
    return (fio.get() & (1 << (block_id % 8))) != 0;
}

int find_empty_block() {
    // Warning: very slow implementation: good optimizations are possible
    // TODO make faster
    for (int block_id = 0; block_id < n_data_blocks; ++block_id) {
        if (!block_used(block_id)) {
            return block_id;
        }
    }
    return -1;
}

int find_descriptor_block_id(const string& filename) {
    File root_dir{root_link};
    int dir_size = root_dir.size();
    vector<char> data(static_cast<size_t>(root_dir.size()));
    root_dir.read(data.data(), dir_size, 0);
    assert(dir_size % sizeof(Link) == 0);

    for (int n_file = 0; n_file < dir_size / sizeof(Link); ++n_file) {
        auto& lnk = *reinterpret_cast<Link*>(data.data() + n_file * sizeof(Link));
        if (lnk.filename == filename) {
            return lnk.descr_block_id;
        }
    }
    return BAD_BLOCK;
}

} // END OF INTERNAL LINKAGE SECTION


bool mount(const string& filename) {
    umount();
    fio.open(filename, fstream::in | fstream::binary | fstream::out);
    if (fio.fail()) {
        return false;
    }

    // measure device capacity
    fio.seekg(0, fio.end);
    device_capacity = fio.tellg();

    // measure how many blocks are used for bitmask
    n_bitmask_blocks = (device_capacity - 1) / (BLOCK_SIZE * BLOCK_SIZE * 8) + 1;
    n_data_blocks = (device_capacity - 1) / BLOCK_SIZE + 1 - n_bitmask_blocks;
    root_link.descr_block_id = 0; // use the first block after bitmask
    root_link.filename[0] = '\0';

    // if first time (device not formatted)
    if (!block_used(root_link.descr_block_id)) {
        // FORMAT IT! (via creating root directory)
        block_mark_used(root_link.descr_block_id);

        char root_data[BLOCK_SIZE];
        read_block(root_link.descr_block_id, root_data);

        auto& root_descr = *reinterpret_cast<Descriptor*>(root_data);
        root_descr.n_links = 1;
        root_descr.size = 0;
        root_descr.type = FileType::Directory;

        write_block(root_link.descr_block_id, root_data);
    }

    return true;
}

void umount() {
    device_capacity = -1;
    n_bitmask_blocks = -1;
    n_data_blocks = -1;
    fio.close();
}

string ls() {
    File root_dir{root_link};
    int dir_size = root_dir.size();
    vector<char> data(static_cast<size_t>(root_dir.size()));
    root_dir.read(data.data(), dir_size, 0);
    assert(dir_size % sizeof(Link) == 0);
    string result;
    for (int n_file = 0; n_file < dir_size / sizeof(Link); ++n_file) {
        auto& lnk = *reinterpret_cast<Link*>(data.data() + n_file * sizeof(Link));
        result += lnk.filename;
        result += '\n';
    }

    return result;
}

bool create(const string& filename, FileType type) {
    if (filename.size() > FILENAME_MAX_LENGTH) {
        return false;
    }
    if (find_descriptor_block_id(filename) != BAD_BLOCK) {
        return false;
    }
    File root_dir{root_link};
    int old_size = root_dir.size();
    root_dir.truncate(old_size + sizeof(Link));

    int descr_block_id = find_empty_block();
    if (descr_block_id == -1) {
        return false;
    }
    block_mark_used(descr_block_id);
    // Create link in root directory
    Link lnk{};
    lnk.descr_block_id = descr_block_id;
    strcpy(lnk.filename, filename.c_str());
    root_dir.write(reinterpret_cast<char*>(&lnk), sizeof(Link), old_size);

    Descriptor descr{};
    descr.size = 0;
    descr.n_links = 1;
    descr.type = FileType::Regular;
    write_block(descr_block_id, reinterpret_cast<char*>(&descr));
    return true;
}

bool link(const string& target, const string& name) {
    if (name.size() > FILENAME_MAX_LENGTH) {
        return false;
    }
    if (find_descriptor_block_id(name) != BAD_BLOCK) {
        return false;
    }
    File root_dir{root_link};
    int dir_size = root_dir.size();
    vector<char> data(static_cast<size_t>(root_dir.size()));
    root_dir.read(data.data(), dir_size, 0);
    assert(dir_size % sizeof(Link) == 0);

    for (int n_file = 0; n_file < dir_size / sizeof(Link); ++n_file) {
        auto& lnk = *reinterpret_cast<Link*>(data.data() + n_file * sizeof(Link));
        if (lnk.filename == target) {
            strcpy(lnk.filename, name.c_str());
            root_dir.truncate(dir_size + sizeof(Link));
            root_dir.write(reinterpret_cast<char*>(&lnk), sizeof(Link), dir_size);

            // add link
            Descriptor descr;
            read_block(lnk.descr_block_id, reinterpret_cast<char*>(&descr));
            descr.n_links += 1;
            write_block(lnk.descr_block_id, reinterpret_cast<char*>(&descr));
            return true;
        }
    }
    return false;
}

bool unlink(const string& filename) {
    // todo
    return false;
}

File::File(const std::string& name): block_id(find_descriptor_block_id(name)) {

}

File::File(const Link& link): block_id(link.descr_block_id) {
    assert(is_mounted());
}

string File::filestat() const {
    assert(is_mounted());
    char descr_data[BLOCK_SIZE];
    read_block(block_id, descr_data);
    auto& descr = *reinterpret_cast<Descriptor*>(descr_data);
    string result = "Type: ";
    result += (descr.type == FileType::Regular ? "regular" : "directory");
    result += '\n';


    result += "Inode: ";
    result += to_string(block_id);
    result += '\n';

    result += "Size: ";
    result += to_string(descr.size);
    result += " bytes";
    result += '\n';

    result += "Number of (hard) links: ";
    result += to_string(descr.n_links);
    result += '\n';
    return result;
}

void File::read(char* data, int size, int shift) const {
    assert(is_mounted());
    char descr_data[BLOCK_SIZE];
    read_block(block_id, descr_data);
    auto& descr = *reinterpret_cast<Descriptor*>(descr_data);
    assert(0 <= size);
    assert(0 <= shift);
    assert(shift + size <= descr.size);
    int index = 0;
    while (size > 0) {
        int block_index = shift / BLOCK_SIZE;
        int& block_id = descr.data_block_ids[block_index];
        int s = min(size, ((block_index + 1) * BLOCK_SIZE) - shift);
        if (block_id != ZERO_BLOCK) {
            read_block(block_id, data + index, s, shift % BLOCK_SIZE);
        } else {
            // zero data optimization (only nulls in file block)
            fill(data + index, data + index + s, '\0');
        }
        shift += s;
        size -= s;
        index += s;
    }
}

bool File::write(const char* data, int size, int shift) {
    assert(is_mounted());
    char descr_data[BLOCK_SIZE];
    read_block(block_id, descr_data);
    auto& descr = *reinterpret_cast<Descriptor*>(descr_data);
    assert(0 <= size);
    assert(0 <= shift);
    assert(shift + size <= descr.size);
    int index = 0;
    bool descriptor_updated = false;
    while (size > 0) {
        int block_index = shift / BLOCK_SIZE;
        int& block_id = descr.data_block_ids[block_index];
        if (block_id == ZERO_BLOCK) {
            block_id = find_empty_block();
            if (block_id == -1) {
                return false;
            }
            block_mark_used(block_id);
            descriptor_updated = true;
        }
        int s = min(size, ((block_index + 1) * BLOCK_SIZE) - shift);
        write_block(block_id, data + index, s, shift % BLOCK_SIZE);
        shift += s;
        size -= s;
        index += s;
    }
    if (descriptor_updated) {
        write_block(block_id, descr_data);
    }
    return true;
}

int File::size() const {
    assert(is_mounted());
    char descr_data[BLOCK_SIZE];
    read_block(block_id, descr_data);
    return reinterpret_cast<Descriptor*>(descr_data) -> size;
}

bool File::truncate(int size) {
    assert(is_mounted());
    char descr_data[BLOCK_SIZE];
    read_block(block_id, descr_data);
    auto& descr = *reinterpret_cast<Descriptor*>(descr_data);
    if (size == descr.size) return true;

    int old_size = descr.size;
    int n_old_blocks = old_size == 0 ? 0 : (old_size - 1) / BLOCK_SIZE + 1;
    int n_blocks = size == 0 ? 0 : (size - 1) / BLOCK_SIZE + 1;
    if (n_blocks < n_old_blocks) {
        for (int block_id = n_blocks; block_id < n_old_blocks; ++block_id) {
            block_mark_unused(descr.data_block_ids[block_id]);
        }
    } else {
        if (descr.size % BLOCK_SIZE != 0 && descr.data_block_ids[n_old_blocks - 1] != ZERO_BLOCK) {
            char tail_data[BLOCK_SIZE];
            read_block(descr.data_block_ids[n_old_blocks - 1], tail_data);
            fill(tail_data + descr.size % BLOCK_SIZE, tail_data + BLOCK_SIZE, '\0');
            write_block(descr.data_block_ids[n_old_blocks - 1], tail_data);
        }
        for (int block_id = n_old_blocks; block_id < n_blocks; ++block_id) {
            descr.data_block_ids[block_id] = ZERO_BLOCK;
        }
    }

    descr.size = size;
    write_block(block_id, descr_data);
    return true;
}

void File::close() const {
    // do nothing
}

File::~File() {
    close();
}

// lab 2
bool mkdir(const string& dirname) {
    // todo
    return false;
}

bool rmdir(const string& dirname) {
    // todo
    return false;
}

bool cd(const string& dirname) {
    // todo
    return false;
}

string pwd() {
    // todo
    if (is_mounted()) {
        return "/";
    } else {
        return "NOT MOUNTED!";
    }
}

bool symlink(const string& name, const string& target) {
    // todo
    return false;
}
} // END OF NAMESPACE myfs
