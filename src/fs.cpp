#include "fs.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>

using namespace std;

namespace myfs {

struct Link {
    int inode_block_id; // relative to the last bitmask block
    char filename[FILENAME_MAX_LENGTH + 1];
};

// INTERNAL LINKAGE SECTION
namespace {

struct INode final {
    FileType type;
    int n_links;
    int size;
    int data_block_ids[BLOCKS_PER_INODE];
    // todo add link to additional data_blocks
};

static_assert(sizeof(INode) != BLOCK_SIZE, "INode size != BLOCK_SIZE");
constexpr int ZERO_BLOCK = -1;
constexpr int BAD_BLOCK = -2;

auto device_capacity = -1;
auto n_bitmask_blocks = -1;
auto n_data_blocks = -1;
fstream fio;
Link root_link;

bool is_mounted();
int div_ceil(int a, int b);
void read_block(int block_id, char* data, int size = BLOCK_SIZE, int shift = 0);
void read_block(int block_id, INode* inode);
void write_block(int block_id, const char* data, int size = BLOCK_SIZE, int shift = 0);
void write_block(int block_id, const INode* inode);
void block_mark_used(int block_id);
void block_mark_unused(int block_id);
bool block_used(int block_id);
int find_empty_block();
int find_inode_block_id(const string& path);
string get_file_directory(const string& path);
string get_filename(const string& path);
int dir_find_file_inode(const File& dir, const string& filename);
int inode_follow_symlinks(int inode_block_id, int max_follows = MAX_SYMLINK_FOLLOWS);
int inode_follow_symlinks(int inode_block_id, int max_follows);

bool is_mounted() {
    return device_capacity != -1 && fio.is_open();
}

int div_ceil(int a, int b) {
    return a == 0 ? 0 : (a - 1) / b + 1;
}

void read_block(int block_id, char* data, int size, int shift) {
    assert(is_mounted());
    assert(0 <= block_id && block_id < n_data_blocks + n_bitmask_blocks);
    assert(0 <= size);
    assert(0 <= shift);
    assert(size + shift <= BLOCK_SIZE);
    fio.seekg(block_id * BLOCK_SIZE + shift, fio.beg);
    fio.read(data, size);
    assert(fio.gcount() == size);
#ifndef NDEBUG
    fio.flush();
#endif
}

void read_block(int block_id, INode* inode) {
    read_block(block_id, reinterpret_cast<char*>(inode));
}

void write_block(int block_id, const char* data, int size, int shift) {
    assert(is_mounted());
    assert(0 <= block_id && block_id < n_data_blocks + n_bitmask_blocks);
    assert(0 <= size);
    assert(0 <= shift);
    assert(size + shift <= BLOCK_SIZE);
    fio.seekp(block_id * BLOCK_SIZE + shift, fio.beg);
    fio.write(data, size);
#ifndef NDEBUG
    fio.flush();
#endif
}

void write_block(int block_id, const INode* inode) {
    write_block(block_id, reinterpret_cast<const char*>(inode));
}

void block_mark_used(int block_id) {
    assert(block_id >= n_bitmask_blocks);
    assert(is_mounted());
    fio.seekg((block_id - n_bitmask_blocks) / 8, fio.beg);
    int old_mask = fio.get();
    assert(old_mask >= 0);
    fio.seekp((block_id - n_bitmask_blocks) / 8, fio.beg);
    fio.put(static_cast<char>(old_mask | (1 << ((block_id - n_bitmask_blocks) % 8))));
#ifndef NDEBUG
    fio.flush();
#endif
}

void block_mark_unused(int block_id) {
    assert(block_id >= n_bitmask_blocks);
    assert(is_mounted());
    fio.seekg((block_id - n_bitmask_blocks) / 8, fio.beg);
    int old_mask = fio.get();
    assert(old_mask >= 0);
    fio.seekp((block_id - n_bitmask_blocks) / 8, fio.beg);
    fio.put(static_cast<char>(old_mask & ~(1 << ((block_id - n_bitmask_blocks) % 8))));
#ifndef NDEBUG
    fio.flush();
#endif
}

bool block_used(int block_id) {
    assert(block_id >= n_bitmask_blocks);
    assert(is_mounted());
    fio.seekg((block_id - n_bitmask_blocks) / 8, fio.beg);
    return (fio.get() & (1 << ((block_id - n_bitmask_blocks) % 8))) != 0;
}

int find_empty_block() {
    for (int bitmask_block_id = 0; bitmask_block_id < n_bitmask_blocks; ++bitmask_block_id) {
        char data[BLOCK_SIZE];
        read_block(bitmask_block_id, data);
        for (int idx = 0; idx < BLOCK_SIZE; ++idx) {
            if (data[idx] != ~'\0') {
                for (int n_bit = 0; n_bit < 8; ++n_bit) {
                    if ((data[idx] & (1 << n_bit)) == 0) {
                        int result = (bitmask_block_id * BLOCK_SIZE + idx) * 8 + n_bit;
                        if (result >= n_data_blocks) {
                            return BAD_BLOCK;
                        }
                        result = result + n_bitmask_blocks;
                        assert(!block_used(result));
                        return result;
                    }
                }
                assert(false);
            }
        }
    }

    return BAD_BLOCK;
}

int dir_find_file_inode(const File& dir, const string& filename) {
    int dir_size = dir.size();
    auto data = dir.cat();
    assert(data.size() % sizeof(Link) == 0);

    for (int n_file = 0; n_file < dir_size / sizeof(Link); ++n_file) {
        auto& lnk = *reinterpret_cast<const Link*>(data.data() + n_file * sizeof(Link));
        if (lnk.filename == filename) {
            return lnk.inode_block_id;
        }
    }
    return BAD_BLOCK;
}

string get_file_directory(const string& path) {
    const auto sep_index = path.find_last_of('/');
    if (sep_index == string::npos) {
        return "/";
    } else {
        return path.substr(0, sep_index);
    }
}

string get_filename(const string& path) {
    const auto sep_index = path.find_last_of('/');
    if (sep_index == string::npos) {
        return path;
    } else {
        return path.substr(sep_index + 1);
    }
}

int find_inode_block_id(const string& path) {
    if (path.size() == 1 && path[0] == DIRECTORY_SEPARATOR) {
        return root_link.inode_block_id;
    }
    int dir_inode = root_link.inode_block_id;
    size_t start = 1;
    if (path[0] != DIRECTORY_SEPARATOR) {
        start = 0;
        // fixme for local paths (add cwd)
    }
    while (true) {
        auto sep_index = path.find(DIRECTORY_SEPARATOR, start);
        if (sep_index == string::npos) {
            auto filename = path.substr(start);
            return dir_find_file_inode(File{dir_inode}, filename);
        } else {
            auto subdirname = path.substr(start, sep_index - start);
            if (subdirname == ".") continue;
            dir_inode =  dir_find_file_inode(File{dir_inode}, subdirname);
            start = sep_index + 1;
            if (dir_inode == BAD_BLOCK) {
                return BAD_BLOCK;
            }
        }
    }
}

int inode_follow_symlinks(int inode_block_id, int max_follows) {
    assert(max_follows >= 0);
    assert(inode_block_id >= 0);
    INode inode;
    read_block(inode_block_id, &inode);
    if (inode.type != FileType::Symlink) {
        return inode_block_id;
    } else {
        if (max_follows == 0) {
            assert(false && "Cyclic reference error");
            return BAD_BLOCK;
        }
        File symlink{inode_block_id, false};
        auto target_name = symlink.cat();
        auto linked_inode_block_id = find_inode_block_id(target_name);
        if (linked_inode_block_id == BAD_BLOCK) {
            assert(false && "Symbolic link points to nothing");
            return BAD_BLOCK;
        }
        return inode_follow_symlinks(find_inode_block_id(target_name), max_follows - 1);
    }
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
    n_bitmask_blocks = div_ceil(device_capacity, BLOCK_SIZE * BLOCK_SIZE * 8);
    n_data_blocks = div_ceil(device_capacity, BLOCK_SIZE);
    root_link.inode_block_id = n_bitmask_blocks; // use the first block after bitmask
    root_link.filename[0] = '\0';

    // if first time (device not formatted)
    if (!block_used(root_link.inode_block_id)) {
        // FORMAT IT! (via creating root directory)
        block_mark_used(root_link.inode_block_id);

        INode root_inode;
        root_inode.n_links = 1;
        root_inode.size = 0;
        root_inode.type = FileType::Directory;
        write_block(root_link.inode_block_id, &root_inode);
    }

    return true;
}

void umount() {
    device_capacity = -1;
    n_bitmask_blocks = -1;
    n_data_blocks = -1;
    fio.close();
}

string ls(const string& dirname) {
    File dir{dirname};
    int dir_size = dir.size();
    vector<char> data(static_cast<size_t>(dir_size));
    dir.read(data.data(), dir_size, 0);
    assert(dir_size % sizeof(Link) == 0);
    string result;
    for (int n_file = 0; n_file < dir_size / sizeof(Link); ++n_file) {
        auto& lnk = *reinterpret_cast<Link*>(data.data() + n_file * sizeof(Link));
        result += lnk.filename;
        result += '\n';
    }

    return result;
}

int create(const string& path, FileType type) {
    if (find_inode_block_id(path) != BAD_BLOCK) {
        return BAD_BLOCK;
    }

    int inode_block_id = find_empty_block();
    if (inode_block_id == BAD_BLOCK) {
        return BAD_BLOCK;
    }

    block_mark_used(inode_block_id);

    auto dirname = get_file_directory(path);
    auto filename = get_filename(path);
    if (filename.size() > FILENAME_MAX_LENGTH) {
        return BAD_BLOCK;
    }
    File dir{dirname};
    int old_dir_size = dir.size();
    dir.truncate(old_dir_size + sizeof(Link));

    // Create link in root directory
    Link lnk;
    lnk.inode_block_id = inode_block_id;
    strcpy(lnk.filename, filename.c_str());
    dir.write(reinterpret_cast<char*>(&lnk), sizeof(Link), old_dir_size);

    INode inode;
    inode.size = 0;
    inode.n_links = 1;
    inode.type = type;
    write_block(inode_block_id, &inode);
    return inode_block_id;
}

bool link(const string& target, const string& name_path) {
    int target_inode = find_inode_block_id(target);
    if (target_inode == BAD_BLOCK || find_inode_block_id(name_path) != BAD_BLOCK) {
        return false;
    }

    auto dirname = get_file_directory(name_path);
    auto filename = get_filename(name_path);
    if (filename.size() > FILENAME_MAX_LENGTH) {
        return BAD_BLOCK;
    }

    File dir{dirname};
    auto s_data = dir.cat();
    vector<char> data(s_data.begin(), s_data.end());
    int old_dir_size = data.size();
    assert(old_dir_size % sizeof(Link) == 0);

    Link lnk;
    strcpy(lnk.filename, filename.c_str());
    lnk.inode_block_id = target_inode;
    dir.truncate(old_dir_size + sizeof(Link));
    dir.write(reinterpret_cast<char*>(&lnk), sizeof(Link), old_dir_size);

    // add link
    INode inode;
    read_block(target_inode, &inode);
    inode.n_links += 1;
    write_block(target_inode, &inode);
    return true;
}

bool unlink(const string& path) {
    int target_inode = find_inode_block_id(path);
    if (target_inode == BAD_BLOCK) {
        return false;
    }

    auto dirname = get_file_directory(path);
    auto filename = get_filename(path);

    if (path.size() > FILENAME_MAX_LENGTH) {
        return false;
    }

    File dir{dirname};
    auto s_data = dir.cat();
    vector<char> data(s_data.begin(), s_data.end());
    int old_dir_size = data.size();
    assert(old_dir_size % sizeof(Link) == 0);
    for (int n_file = 0; n_file < old_dir_size / sizeof(Link); ++n_file) {
        auto& lnk = *reinterpret_cast<Link*>(data.data() + n_file * sizeof(Link));
        if (lnk.filename == filename) {
            INode inode;
            read_block(lnk.inode_block_id, &inode);

            if (inode.n_links == 1) {
                for (int block_index = 0; block_index * BLOCK_SIZE < inode.size; ++block_index) {
                    int block_id = inode.data_block_ids[block_index];
                    if (block_id != ZERO_BLOCK) {
                        block_mark_unused(block_id);
                    }
                }
                block_mark_unused(lnk.inode_block_id);
            } else {
                --inode.n_links;
                write_block(lnk.inode_block_id, &inode);
            }

            dir.read(reinterpret_cast<char*>(&lnk), sizeof(Link), old_dir_size - sizeof(Link));
            dir.write(reinterpret_cast<char*>(&lnk), sizeof(Link), n_file * sizeof(Link));
            dir.truncate(old_dir_size - sizeof(Link));
            return true;
        }
    }

    return false;
}

bool file_exists(const string& filename) {
    return find_inode_block_id(filename) != BAD_BLOCK;
}

File::File(const string& filename, bool follow_symlink) : File(find_inode_block_id(filename), follow_symlink) {

}

File::File(int block_id, bool follow_symlink):
        block_id{follow_symlink ? inode_follow_symlinks(block_id) : block_id } {
    assert(block_id >= 0 && "file not found");
    assert(is_mounted());
}

string File::filestat() const {
    assert(is_mounted());

    INode inode;
    read_block(block_id, &inode);

    string result = "Type: ";
    if (inode.type == FileType::Regular) {
        result += "regular";
    } else if (inode.type == FileType::Symlink) {
        result += "symlink\n";
        result += "Points to: ";
        result += cat();
    } else {
        // directory
        result += "directory\n";
        result += "Contains files: ";
        result += to_string(inode.size / sizeof(Link));
    }
    result += '\n';


    result += "Inode: ";
    result += to_string(block_id);
    result += '\n';

    result += "Blocks uses(";
    string blocks;
    int blocks_used = 0;
    for (int i = 0; i < div_ceil(inode.size, BLOCK_SIZE); ++i) {
        if (inode.data_block_ids[i] >= 0) {
            blocks += '#';
            blocks += to_string(inode.data_block_ids[i]);
            blocks += ' ';
            ++blocks_used;
        }
    }
    result += to_string(blocks_used);
    result += "): ";
    result += blocks;
    result += '\n';

    result += "Size: ";
    result += to_string(inode.size);
    result += " bytes";
    result += '\n';

    result += "Number of (hard) links: ";
    result += to_string(inode.n_links);
    result += '\n';

    return result;
}

void File::read(char* data, int size, int shift) const {
    assert(is_mounted());
    INode inode;
    read_block(block_id, &inode);
    assert(0 <= size);
    assert(0 <= shift);
    assert(shift + size <= inode.size);
    int index = 0;
    while (size > 0) {
        int block_index = shift / BLOCK_SIZE;
        int& block_id = inode.data_block_ids[block_index];
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

string File::cat() const {
    vector<char> data(static_cast<size_t>(size()));
    read(data.data(), data.size(), 0);
    string result(data.begin(), data.end());
    assert(result.size() == data.size());
    return result;
}

bool File::write(const char* data, int size, int shift) {
    assert(is_mounted());
    INode inode;
    read_block(block_id, &inode);
    assert(0 <= size);
    assert(0 <= shift);
    assert(shift + size <= inode.size);
    int index = 0;
    bool inode_updated = false;
    while (size > 0) {
        int next_block_index = shift / BLOCK_SIZE;
        int& next_block_id = inode.data_block_ids[next_block_index];
        if (next_block_id == ZERO_BLOCK) {
            next_block_id = find_empty_block();
            if (next_block_id == BAD_BLOCK) {
                inode.size = shift;
                write_block(block_id, &inode);
                return false;
            }
            block_mark_used(next_block_id);
            inode_updated = true;
        }
        int s = min(size, ((next_block_index + 1) * BLOCK_SIZE) - shift);
        write_block(next_block_id, data + index, s, shift % BLOCK_SIZE);
        shift += s;
        size -= s;
        index += s;
    }
    if (inode_updated) {
        write_block(block_id, &inode);
    }
    return true;
}

int File::size() const {
    assert(is_mounted());
    INode inode;
    read_block(block_id, &inode);
    return inode.size;
}

FileType File::type() const {
    assert(is_mounted());
    INode inode;
    read_block(block_id, &inode);
    return inode.type;
}

bool File::truncate(int size) {
    assert(is_mounted());
    INode inode;
    read_block(block_id, &inode);

    if (size == inode.size) return true;

    int n_old_blocks = div_ceil(inode.size, BLOCK_SIZE);
    int n_blocks = div_ceil(size, BLOCK_SIZE);
    assert(n_blocks <= BLOCKS_PER_INODE && "too big file");
    if (n_blocks < n_old_blocks) {
        for (int block_index = n_blocks; block_index < n_old_blocks; ++block_index) {
            if (inode.data_block_ids[block_index] >= 0) {
                block_mark_unused(inode.data_block_ids[block_index]);
            }
        }
    } else {
        if (inode.size % BLOCK_SIZE != 0 && inode.data_block_ids[n_old_blocks - 1] != ZERO_BLOCK) {
            char tail_data[BLOCK_SIZE];
            read_block(inode.data_block_ids[n_old_blocks - 1], tail_data);
            fill(tail_data + inode.size % BLOCK_SIZE, tail_data + BLOCK_SIZE, '\0');
            write_block(inode.data_block_ids[n_old_blocks - 1], tail_data);
        }
        fill(inode.data_block_ids + n_old_blocks, inode.data_block_ids + n_blocks, ZERO_BLOCK);
    }

    inode.size = size;
    write_block(block_id, &inode);
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
    return create(dirname, FileType::Directory) != BAD_BLOCK;
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

bool symlink(const string& target, const string& name) {
    int inode_block_id = create(name, FileType::Symlink);
    if (inode_block_id == BAD_BLOCK) {
        return false;
    }
    File file{inode_block_id, false};
    file.truncate(target.size());
    file.write(target.c_str(), target.size(), 0);
    return true;
}
} // END OF NAMESPACE myfs
