#include "gtest/gtest.h"
#include "../DirectoryWatcherWin32.h"
#include "../FileSystem.h"
#include "DirectoryTree.h"
#include <queue>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <iostream>

namespace
{
    using namespace YAM;
    using namespace YAMTest;
    using path = std::filesystem::path;
    using FA = FileChange::Action;

    bool contains(std::vector<FileChange> changes, FileChange change) {
        for (auto const& c : changes) {
            if (
                c.action == change.action
                && c.fileName == change.fileName
                && c.oldFileName == change.oldFileName
             ) {
                return true;
            }
        }
        return false;
    }

    // This test demonstrates spurious change events on first-time iterating
    // over just created directories in the Windows implementation
    // According to ReadDirectoryChangesW for FILE_NOTIFY_CHANGE_LAST_WRITE:
    //    Any change to the last-write-time of files in the watched directory 
    //    or subtree causes a change notification wait operation to return. The 
    //    operating system detects a change to the last-write-time only when the 
    //    file is written to the disk.For operating systems that use extensive caching,
    //    detection occurs only when the cache is sufficiently flushed.
    // It seems that flushing the directory files to cache is triggered by the
    // iteration over these directories. This is confirmed by the following experiment:
    //      - create directory tree
    //      - flush filesystem cache using sysinternals sync.exe
    //      - start watching the directory tree
    //      - iterate the directories.
    TEST(DirectoryWatcherWin32, spuriousChangeEvents) {
        std::filesystem::path rootDir= FileSystem::createUniqueDirectory();
        std::vector<FileChange> detectedChanges;
        std::mutex mutex;
        std::condition_variable cond;
        DirectoryTree testTree(rootDir, 3, RegexSet());

        auto watcher = std::make_shared<DirectoryWatcherWin32>(
            rootDir, true,
            Delegate<void, FileChange const&>::CreateLambda([&](FileChange const& c)
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    detectedChanges.push_back(c);
                    cond.notify_one();
                })
        );
        watcher->start();

        // Now iterate some directories in testTree.
        // Although no changes are expected there are changes for three directories:
        // rootDir/SubDir2/SubDir1..3
        std::filesystem::path s2(rootDir / "SubDir2");
        for (auto const& entry : std::filesystem::directory_iterator(s2)) {
            if (entry.is_directory()) {
                std::filesystem::path s3(s2 / entry.path());
                for (auto const& subentry : std::filesystem::directory_iterator(s3)) {
                }
            }
        }

        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        std::unique_lock<std::mutex> lock(mutex);
        bool done;
        do {
            done = 3 == detectedChanges.size();
        } while (!done && cond.wait_until(lock, deadline) != std::cv_status::timeout);
        EXPECT_EQ(3,  detectedChanges.size());

        // Repeat the above, now no changes found.
        detectedChanges.clear();
        for (auto const& entry : std::filesystem::directory_iterator(s2)) {
            if (entry.is_directory()) {
                std::filesystem::path s3(s2 / entry.path());
                for (auto const& subentry : std::filesystem::directory_iterator(s3)) {
                }
            }
        }
        deadline = std::chrono::system_clock::now() + std::chrono::seconds(1);
        do {
            done = !detectedChanges.empty();
        } while (!done && cond.wait_until(lock, deadline) != std::cv_status::timeout);
        EXPECT_EQ(0, detectedChanges.size());

        lock.unlock();
        watcher->stop();
        std::filesystem::remove_all(rootDir);
    }

    TEST(DirectoryWatcherWin32, suppressSpuriousChangeEvents) {
        std::filesystem::path rootDir = FileSystem::createUniqueDirectory();
        std::vector<FileChange> detectedChanges;
        std::mutex mutex;
        std::condition_variable cond;
        DirectoryTree testTree(rootDir, 3, RegexSet());

        auto watcher = std::make_shared<DirectoryWatcherWin32>(
            rootDir, true,
            Delegate<void, FileChange const&>::CreateLambda([&](FileChange const& c)
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    detectedChanges.push_back(c);
                    cond.notify_one();
                }),
            true
        );
        watcher->start();

        // Now iterate some directories in testTree.
        // No spurious events should be notified.
        std::filesystem::path s2(rootDir / "SubDir2");
        for (auto const& entry : std::filesystem::directory_iterator(s2)) {
            if (entry.is_directory()) {
                std::filesystem::path s3(s2 / entry.path());
                for (auto const& subentry : std::filesystem::directory_iterator(s3)) {
                }
            }
        }

        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(1);
        std::unique_lock<std::mutex> lock(mutex);
        bool done;
        do { 
            done = !detectedChanges.empty();
        } while (!done && cond.wait_until(lock, deadline) != std::cv_status::timeout);
        EXPECT_EQ(0, detectedChanges.size());

        lock.unlock();
        watcher->stop();
        std::filesystem::remove_all(rootDir);
    }
    TEST(DirectoryWatcherWin32, update_DirectoryTree) {
        std::filesystem::path rootDir = FileSystem::createUniqueDirectory();
        std::vector<FileChange> detectedChanges;
        std::mutex mutex;
        std::condition_variable cond;
        DirectoryTree testTree(rootDir, 3, RegexSet());
        DirectoryTree* sd2 = testTree.getSubDirs()[1];
        DirectoryTree* sd2_sd3 = sd2->getSubDirs()[2];

        auto watcher = std::make_shared<DirectoryWatcherWin32>(
            rootDir, true,
            Delegate<void, FileChange const&>::CreateLambda([&](FileChange const& c)
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    detectedChanges.push_back(c);
                    cond.notify_one();
                })
        );
        watcher->start();

        // Update file system and create expected changes
        testTree.addFile();
        FileChange c1a{ FA::Added, path(rootDir / "File4") };

        sd2->addFile();
        FileChange c2a{ FA::Modified, path(rootDir / "SubDir2") };
        FileChange c2b{ FA::Added, path(rootDir / "SubDir2\\File4") };
        
        sd2_sd3->addDirectory();
        FileChange c3a{ FA::Modified, path(rootDir / "SubDir2\\SubDir3") };
        FileChange c3b{ FA::Added, path(rootDir / "SubDir2\\SubDir3\\SubDir4") };
        FileChange c3c{ FA::Modified, path(rootDir / "SubDir2\\SubDir3\\SubDir4") };
        FileChange c3d{ FA::Added, path(rootDir / "SubDir2\\SubDir3\\SubDir4\\File1") };
        FileChange c3e{ FA::Added, path(rootDir / "SubDir2\\SubDir3\\SubDir4\\File2") };
        FileChange c3f{ FA::Added, path(rootDir / "SubDir2\\SubDir3\\SubDir4\\File3") };
        FileChange c3g{ FA::Added, path(rootDir / "SubDir2\\SubDir3\\SubDir4\\SubDir1") };
        FileChange c3h{ FA::Added, path(rootDir / "SubDir2\\SubDir3\\SubDir4\\SubDir2") };
        FileChange c3i{ FA::Added, path(rootDir / "SubDir2\\SubDir3\\SubDir4\\SubDir3") };

        sd2_sd3->addFile();
        FileChange c4a{ FA::Added, path(rootDir / "SubDir2\\SubDir3\\File4") };
        FileChange c4b{ FA::Modified, path(rootDir / "SubDir2\\SubDir3") };

        sd2_sd3->modifyFile("File4");
        FileChange c5a{ FA::Modified, path(rootDir / "SubDir2\\SubDir3\\File4") };

        sd2_sd3->renameFile("File4", "File5");
        FileChange c6a{ FA::Renamed, path(rootDir / "SubDir2\\SubDir3\\File5"), path(rootDir / "SubDir2\\SubDir3\\File4") };

        sd2_sd3->deleteFile("File1");
        FileChange c7a{ FA::Removed, path(rootDir / "SubDir2\\SubDir3\\File1") };
        FileChange c7b{ FA::Modified, path(rootDir / "SubDir2\\SubDir3\\File1") };
        
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);       
        std::vector<FileChange>& dcs = detectedChanges;
        std::unique_lock<std::mutex> lock(mutex);
        bool done = false;
        do {
            done =
                (contains(dcs, c1a))
                && (contains(dcs, c2a) || contains(dcs, c2b))
                && (contains(dcs, c3a) || contains(dcs, c3b))
                && (contains(dcs, c3c) || contains(dcs, c3d))
                && (contains(dcs, c3c) || contains(dcs, c3e))
                && (contains(dcs, c3c) || contains(dcs, c3f))
                && (contains(dcs, c3c) || contains(dcs, c3g))
                && (contains(dcs, c3c) || contains(dcs, c3h))
                && (contains(dcs, c3c) || contains(dcs, c3i))
                && (contains(dcs, c4a) || contains(dcs, c4b))
                && (contains(dcs, c5a))
                && (contains(dcs, c6a))
                && (contains(dcs, c7a) || contains(dcs, c7b))
                ;
        } while (!done && cond.wait_until(lock, deadline) != std::cv_status::timeout);

        EXPECT_TRUE(done);
        EXPECT_TRUE(contains(dcs, c1a));
        EXPECT_TRUE(contains(dcs, c2a) || contains(dcs, c2b));
        EXPECT_TRUE(contains(dcs, c3a) || contains(dcs, c3b));
        EXPECT_TRUE(contains(dcs, c3c) || contains(dcs, c3d));
        EXPECT_TRUE(contains(dcs, c3c) || contains(dcs, c3e));
        EXPECT_TRUE(contains(dcs, c3c) || contains(dcs, c3f));
        EXPECT_TRUE(contains(dcs, c3c) || contains(dcs, c3g));
        EXPECT_TRUE(contains(dcs, c3c) || contains(dcs, c3h));
        EXPECT_TRUE(contains(dcs, c3c) || contains(dcs, c3i));
        EXPECT_TRUE(contains(dcs, c4a) || contains(dcs, c4b));
        EXPECT_TRUE(contains(dcs, c5a));
        EXPECT_TRUE(contains(dcs, c6a));
        EXPECT_TRUE(contains(dcs, c7a) || contains(dcs, c7b));

        lock.unlock();
        watcher->stop();
        std::filesystem::remove_all(rootDir);
    }
}