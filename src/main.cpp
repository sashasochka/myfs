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
            cout << (myfs::create(filename) ? "File created" : "File wasn't created") << endl;
        } else if (cmd == "link" || cmd == "ln") {
            string target, name;
            cin >> target >> name;
            cout << (myfs::link(target, name) ? "Link created" : "Link wasn't created") << endl;
        } else if (cmd == "unlink" || cmd == "rm") {
            string filename;
            cin >> filename;
            cout << (myfs::unlink(filename) ? "Hard link was removed" : "Hard link wasn't removed") << endl;
        } else if (cmd == "mkdir") {
            string dirname;
            cin >> dirname;
            cout << (myfs::mkdir(dirname) ? "Dir created" : "Dir wasn't created") << endl;
        } else if (cmd == "rmdir") {
            string dirname;
            cin >> dirname;
            cout << (myfs::rmdir(dirname) ? "Dir successfully removed" : "Dir wasn't removed") << endl;
        } else if (cmd == "cd") {
            string dirname;
            cin >> dirname;
            cout << (myfs::cd(dirname) ? "cwd changed" : "No such directory") << endl;
        } else if (cmd == "pwd") {
            cout << myfs::pwd() << endl;
        } else if (cmd == "symlink") {
            string target, name;
            cin >> target >> name;
            cout << (myfs::symlink(target, name) ? "Symlink created" : "Symlink wasn't created") << endl;
        } else if (cmd == "filestat") {
            string filename;
            cin >> filename;
            myfs::File f{filename};
            cout << f.filestat();
        } else if (cmd == "read") {
            string filename;
            cin >> filename;
//            myfs::File f{filename};
//            cout << f.filestat() << endl;
            cout << "NOT IMPLEMENTED" << endl;
        } else if (cmd == "write") {
            string filename;
            cin >> filename;
//            myfs::File f{filename};
//            cout << f.filestat() << endl;
            cout << "NOT IMPLEMENTED" << endl;
        } else if (cmd == "truncate") {
            string filename;
            int size;
            cin >> filename;
            cin >> size;
            myfs::File f{filename};
            cout << (f.truncate(size) ? "File was truncated" : "File wasn't trucated") << endl;
        } else {
            cout << "Uknown command!" << endl;
        }
    }
    myfs::umount();
    return EXIT_SUCCESS;
}
