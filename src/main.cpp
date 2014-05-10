#include "fs.h"

#include <iostream>

using namespace std;

int main() {
    while (true) {
        cin.clear();
        string cmd;
        cin >> cmd;
        if (cin.eof()) {
            break;
        }
        if (cmd == "mount") {
            string fsFileName;
            cin >> fsFileName;
            if (myfs::mount(fsFileName)) {
                cout << "File system mounted!" << endl;
            } else {
                cout << "Cannot mount file system!" << endl;
            }
        } else if (cmd == "umount") {
            myfs::umount();
            cout << "File system unmounted!" << endl;
        } else if (cmd == "ls") {
            cout << myfs::ls();
        } else if (cmd == "create" || cmd == "touch") {
            string filename;
            cin >> filename;
            if (!myfs::file_exists(filename)) {
                cout << (myfs::create(filename) >= 0 ? "File created" : "File wasn't created") << endl;
            } else {
                cout << "File already exists!" << endl;
            }
        } else if (cmd == "link" || cmd == "ln") {
            string target, name;
            cin >> target >> name;
            if (!myfs::file_exists(target)) {
                cout << "Target file doesn't exist" << endl;
            } else if (myfs::file_exists(name)) {
                cout << "File with name '" << name << "' already exists" << endl;
            } else {
                cout << (myfs::link(target, name) ? "Link created" : "Link wasn't created") << endl;
            }
        } else if (cmd == "unlink" || cmd == "rm") {
            string filename;
            cin >> filename;
            if (!myfs::file_exists(filename)) {
                cout << "File doesn't exist" << endl;
            } else if (myfs::File(filename, false).type() == myfs::FileType::Directory) {
                cout << "Cannot remove directory, use `rmdir` command" << endl;
            } else {
                cout << (myfs::unlink(filename) ? "Hard link was removed" : "Hard link wasn't removed") << endl;
            }
        } else if (cmd == "mkdir") {
            string dirname;
            cin >> dirname;
            if (myfs::file_exists(dirname)) {
                cout << "File with name '" << dirname << "' already exists" << endl;
            } else {
                cout << (myfs::mkdir(dirname) ? "Dir created" : "Dir wasn't created") << endl;
            }
        } else if (cmd == "rmdir") {
            string dirname;
            cin >> dirname;
            if (myfs::file_exists(dirname)) {
                cout << (myfs::rmdir(dirname) ? "Dir successfully removed" : "Dir wasn't removed") << endl;
            } else {
                cout << "Directory doesn't exist";
            }
        } else if (cmd == "cd") {
            string dirname;
            cin >> dirname;
            cout << (myfs::cd(dirname) ? "cwd changed" : "No such directory") << endl;
        } else if (cmd == "pwd") {
            cout << myfs::pwd() << endl;
        } else if (cmd == "symlink") {
            string target, name;
            cin >> target >> name;

            if (!myfs::file_exists(target)) {
                cout << "Target file doesn't exist" << endl;
            } else if (myfs::file_exists(name)) {
                cout << "File with name '" << name << "' already exists" << endl;
            } else {
                cout << (myfs::symlink(target, name) ? "Symlink created" : "Symlink wasn't created") << endl;
            }
        } else if (cmd == "filestat" || cmd == "stat") {
            string filename;
            cin >> filename;
            if (myfs::file_exists(filename)) {
                myfs::File f{filename, false};
                cout << f.filestat();
            } else {
                cout << "File with name '" << filename << "' doesn't exist" << endl;
            }
        } else if (cmd == "read" || cmd == "cat") {
            string filename;
            cin >> filename;
            if (myfs::file_exists(filename)) {
                cout << myfs::File{filename}.cat() << endl;
            } else {
                cout << "File with name '" << filename << "' doesn't exist" << endl;
            }
        } else if (cmd == "write") {
            string filename;
            cin >> filename;
            string data;
            bool first = true;
            while (true) {
                string s;
                cin >> s;
                if (s == "END") break;
                if (!first) {
                    data += ' ';
                }
                first = false;
                data += s;
            }

            if (myfs::file_exists(filename)) {
                myfs::File f{filename};
                f.truncate(data.size());
                if (f.write(data.data(), data.size(), 0)) {
                    cout << "Data successfully written" << endl;
                } else {
                    cout << "Cannot write data (probably not enough space)" << endl;
                }

            } else {
                cout << "File with name '" << filename << "' doesn't exist" << endl;
            }
        } else if (cmd == "truncate") {
            string filename;
            int size;
            cin >> filename;
            cin >> size;
            if (cin.fail()) continue;
            if (myfs::file_exists(filename)) {
                myfs::File f{filename};
                cout << (f.truncate(size) ? "File was truncated" : "File wasn't trucated") << endl;
            } else {
                cout << "File with name '" << filename << "' doesn't exist" << endl;
            }
        } else {
            cout << "Uknown command!" << endl;
        }
    }
    myfs::umount();
    return EXIT_SUCCESS;
}
