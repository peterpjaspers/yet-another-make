#include "FileAccessEvents.h"

#include <map>
#include <mutex>
#include <condition_variable> 
#include <queue> 

using namespace std;

// ToDo: Inter-process FileAccessEvent queue

namespace AccessMonitor {

    namespace {

        struct FileAccessEvent {
            wstring fileName;
            FileAccessMode accessMode;
            FileTime lastWriteTime;
            FileAccessEvent() = delete;
            FileAccessEvent( const wstring& f, FileAccessMode m, const FileTime& t ) : fileName( f ), accessMode( m ), lastWriteTime( t ) {}
        };

        // Thread-safe file access event queue
        // FileAccessEvent object must be allocated by the producer and freed (deleted) by the consumer.
        // The queue itself holds const pointers to the the event objects in the queue.
        class FileAccessQueue { 
        private: 
            queue<const FileAccessEvent*> eventQueue; 
            mutex queueMutex; 
            condition_variable condition; 
        public: 
            // Pushes a file access event on the queue 
            void push( const FileAccessEvent* event ) { 
                unique_lock<mutex> lock( queueMutex ); 
                eventQueue.push( event ); 
                condition.notify_one(); 
            }
            // Stop collecting file access events.
            // All events in the queue will collected.
            inline void stop() {
                push( new FileAccessEvent( wstring(), AccessStopMonitoring, FileTime() ) );
            }
            // Pops a file access event off the queue.
            // Caller is obliged to delete the event.
            const FileAccessEvent* pop()  { 
                unique_lock<mutex> lock(queueMutex); 
                condition.wait( lock, [this]() { return !eventQueue.empty(); } ); 
                const FileAccessEvent* event = eventQueue.front(); 
                eventQueue.pop(); 
                return event; 
            }
            void clear() {
                unique_lock<mutex> lock( queueMutex );
                while (!eventQueue.empty()) {
                    const FileAccessEvent* event = eventQueue.front(); 
                    eventQueue.pop(); 
                    delete event;
                }
            }
        };

        FileAccessQueue fileAccessQueue;

    }

    FileAccessState::FileAccessState() : accessedModes( AccessNone ) {}
    FileAccessState::FileAccessState( const FileAccessMode& m, const FileTime& t ) : accessedModes( m ), lastWriteTime( t ) {}

    // Convert FileAccessMode value to string
    wstring modeString( FileAccessMode mode ) {
        auto s = wstring( L"" );
        if ((mode & AccessRead) != 0) s += L"Read ";
        if ((mode & AccessWrite) != 0) s += L"Write ";
        if ((mode & AccessDelete) != 0) s += L"Delete ";
        if ((mode & AccessVariable) != 0) s += L"Variable ";
        return s;
    }

    // Record a file access event
    void recordFileEvent( const wstring& f, FileAccessMode m, const FileTime& t ) {
        fileAccessQueue.push( new FileAccessEvent( f, m, t ) );
    }

    // Collect files events recorded by this process
    const map< wstring, FileAccessState > collectFileEvents() {
        fileAccessQueue.push( new FileAccessEvent( wstring(), AccessStopMonitoring, FileTime() ) );
        map< wstring, FileAccessState > accesses;
        bool stop = false;
        while (!stop) {
            const FileAccessEvent* event = fileAccessQueue.pop();
            if ((event->accessMode & AccessStopMonitoring) == 0) {

                auto access = accesses.find( event->fileName );
                if ( access == accesses.end() ) {
                    accesses[ event->fileName ] = FileAccessState( event->accessMode, event->lastWriteTime );
                } else {
                    FileAccessState& state = access->second;
                    state.accessedModes |= event->accessMode;
                    if (state.lastWriteTime < event->lastWriteTime) state.lastWriteTime = event->lastWriteTime;
                }
            } else stop = true;
            delete event;
        }
        fileAccessQueue.clear();
        return accesses;
    }
    void streamAccessedFiles( wostream& stream ) {
        const map< wstring, FileAccessState > accessedFiles = collectFileEvents();
        for ( auto access : accessedFiles ) {
            const wstring& file = access.first;
            const FileAccessState& data = access.second;
            stream << file << " [ " << data.lastWriteTime << " ] " << modeString( data.accessedModes ) << "\n";
        }
    };

} // namespace AccessMonitor
