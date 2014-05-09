#include <string>
#include <vector>

namespace myfs
{
constexpr auto
    BLOCK_SIZE = 512, // 64 (512)
    FILENAME_MAX_LENGTH = 15,
    BLOCKS_PER_INODE = 126; // 14 (126)

struct Link;

enum class FileType { Regular, Directory, Symlink };

struct File final {
    File(const std::string& name);
    File(const Link& link);
    std::string filestat() const;
    void read(char* data, int size, int shift) const;
    std::string cat() const;
    bool write(const char* data, int size, int shift);
    int size() const;
    bool truncate(int size);
    void close() const;
    ~File();
private:
    const int block_id;
};

bool mount(const std::string& filename);
void umount();
std::string ls();
bool create(const std::string& filename, FileType type = FileType::Regular);
bool link(const std::string& target, const std::string& name);
bool unlink(const std::string& filename);
bool mkdir(const std::string& dirname);
bool rmdir(const std::string& dirname);
bool cd(const std::string& dirname);
std::string pwd();
bool symlink(const std::string &name, const std::string &target);
} // END OF NAMESPACE myfs
