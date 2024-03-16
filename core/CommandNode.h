#pragma once

#include "Node.h"
#include "FileNode.h"
#include "IMonitoredProcess.h"
#include "MemoryLogBook.h"
#include "Glob.h"
#include "xxhash.h"

#include <atomic>
#include <unordered_set>

namespace YAM
{
    class GeneratedFileNode;
    class DirectoryNode;
    class FileRepositoryNode;
    class SourceFileNode;
    class GroupNode;

    // CommandNode is capable of executing a shell script and detecting which
    // files were accessed by the script during its execution.
    // Command will:
    //    - check for detected output (i.e. write-accessed) files that:
    //        - all mandatory output files were write-accessed. 
    //        - all non-mandatory output files match the optional or ignored 
    //          output file declarations.
    //        - output files are not also outputs of other CommandNodes
    //        - output files are not source files
    //      Execution fails if one of these checks fails.
    //    - delete write-accessed files that match the ignore ouput declarations.   
    //    - delete optional output files that were no longer detected as outputs.
    //    - register detected optional output files.
    //    - ignore read-accessed files lexically contained in repositories of 
    //      type Ignore.
    //    - register read-accessed files that are lexically contained in file
    //      repositories of type Build and Track as input files.
    //      Also files that do not exist in the filesystem are registered.
    //      This is important to detect addition/removal of C/C++ header files
    //      to/from the list of include file directories in which the compiler
    //      looks for header files
    //    - invalidate the output files when changes in command node inputs 
    //      (e.g. script, working directory, cmd inputs, etc) and/or tampering
    //      with ouput files is detected.
    //      
    class __declspec(dllexport) CommandNode : public Node
    {
    public:
        typedef std::map<std::filesystem::path, std::shared_ptr<FileNode>> InputNodes;
        typedef std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> OutputNodes;
        class __declspec(dllexport) PostProcessor {
        public:
            // Called after successfull completion of script.
            // Called in threadpool context.
            virtual void process(MonitoredProcessResult const& scriptResult) = 0;
        };

        CommandNode(); // needed for deserialization
        CommandNode(ExecutionContext* context, std::filesystem::path const& name);
        ~CommandNode();

        std::string className() const override { return "CommandNode"; }

        // Set the name of input file aspect set. The set is accessed via
        // context()->findFileAspectSet(newName).
        // The command node will only re-execute when input file aspect hash 
        // values have changed since previous command node execution.
        // 
        void inputAspectsName(std::string const& newName);
        std::string const& inputAspectsName(std::string const& newName) const;

        // Pre: newInputs contains SourceFileNode and/or GeneratedFileNode
        // and/or GroupNode instances.
        void cmdInputs(std::vector<std::shared_ptr<Node>> const& newInputs);
        std::vector<std::shared_ptr<Node>> const& cmdInputs() const;

        // Pre: newInputs contains GeneratedFileNode and/or GroupNode instances.
        void orderOnlyInputs(std::vector<std::shared_ptr<Node>> const& newInputs);
        std::vector<std::shared_ptr<Node>> const& orderOnlyInputs() const;

        // Set/get the shell script to be executed when this node executes.
        void script(std::string const& newScript);
        std::string const& script() const;

        // The directory in which the script will be executed.
        // The repository root directory when nullptr.
        void workingDirectory(std::shared_ptr<DirectoryNode> const& dir);
        std::shared_ptr<DirectoryNode> workingDirectory() const;

        void mandatoryOutputs(std::vector<std::shared_ptr<GeneratedFileNode>> const& newOutputs);
        std::vector<std::shared_ptr<GeneratedFileNode>> const& mandatoryOutputs() const;

        // Set/get paths/globs of optional output files. Detected output files 
        // that match one of these paths/globs are registered in 
        // detectedOptionalOutputs().
        void optionalOutputs(std::vector<std::filesystem::path> const& newOutputs);
        std::vector<std::filesystem::path> const& optionalOutputs() const;

        // Set/get paths and/or glob paths. Detected output files that match
        // one of these paths/globs are ignored and deleted.
        void ignoreOutputs(std::vector<std::filesystem::path> const& newOutputs);
        std::vector<std::filesystem::path> const& ignoreOutputs() const;

        // Return optional outputs detected during last script execution.
        // Pre: state() == Node::State::Ok
        OutputNodes const& detectedOptionalOutputs() const;

        // Return union of mandatoryOutputs() and detectedOptionalOutputs() 
        // Pre: state() == Node::State::Ok
        OutputNodes const& detectedOutputs() const;

        // Pre: state() == Node::State::Ok
        // Return inputs detected during last script execution.
        InputNodes const& detectedInputs() const;

        // Return the groups that will be executed before this command executes,
        // i.e. the group nodes in cmdInputs and orderOnlyInputs.
        std::vector<std::shared_ptr<GroupNode>> inputGroups() const;

        // Override Node
        void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;
        void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;

        void postProcessor(std::shared_ptr<PostProcessor> const& processor);
        void start() override;
        void cancel() override;

        void buildFile(SourceFileNode* buildFile);
        void ruleLineNr(std::size_t ruleLineNr);
        SourceFileNode const* buildFile() const;
        std::size_t ruleLineNr() const;

        XXH64_hash_t executionHash() const;

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    protected:
        void cleanup() override;

    private:
        struct ExecutionResult {
            MemoryLogBook _log;
            Node::State _newState;
            std::set<std::filesystem::path> _outputPaths;
            std::set<std::filesystem::path> _keptInputPaths;
            std::set<std::filesystem::path> _removedInputPaths;
            std::set<std::filesystem::path> _addedInputPaths;
            std::vector<std::shared_ptr<FileNode>> _addedInputNodes;
        };

        std::filesystem::path convertToSymbolicPath(std::filesystem::path const& absPath, MemoryLogBook& logBook);
        std::set<std::filesystem::path> convertToSymbolicPaths(
            std::set<std::filesystem::path> const& absPaths,
            MemoryLogBook& logBook);
        std::set<std::filesystem::path> convertToSymbolicPaths(
            std::set<std::filesystem::path> const& absPaths,
            MemoryLogBook& logBook,
            std::vector<std::filesystem::path> const &toIgnore,
            std::vector<std::filesystem::path>& ignored);
        void handleRequisitesCompletion(Node::State state);
        void executeScript();
        void handleExecuteScriptCompletion(std::shared_ptr<ExecutionResult> result);
        void handleOutputAndNewInputFilesCompletion(Node::State newState, std::shared_ptr<ExecutionResult> result);
        void updateInputProducers();
        void notifyCommandCompletion(std::shared_ptr<ExecutionResult> result);

        std::string compileScript(ILogBook& logBook);
        MonitoredProcessResult executeMonitoredScript(ILogBook& logBook);
        void getSourceInputs(std::vector<Node*>& sourceInputs) const;
        void clearDetectedInputs();
        void setDetectedInputs(ExecutionResult const& result);
        void setDetectedOptionalOutputs(
            std::vector<std::shared_ptr<GeneratedFileNode>> const& optionals,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& newOptionals);

        XXH64_hash_t computeExecutionHash() const;

        bool findInputNodes(
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& allowedGenInputFiles,
            std::set<std::filesystem::path>const& inputSymPaths,
            std::vector<std::shared_ptr<FileNode>>& inputNodes,
            std::vector<std::shared_ptr<Node>>& srcInputNodes,
            ILogBook& logBook
        );
        bool findOutputNodes(
            std::set<std::filesystem::path> const& outputSymPaths,
            std::vector<std::shared_ptr<GeneratedFileNode>>& optionalOutputNodes,
            std::vector<std::shared_ptr<GeneratedFileNode>>& newOptionalOutputNodes,
            MemoryLogBook& logBook);
        bool verifyMandatoryOutputs(
            std::vector<std::shared_ptr<GeneratedFileNode>> const& outputNodes,
            MemoryLogBook& logBook);


        // buildFile that contains the rule from which this node is created
        SourceFileNode* _buildFile;
        std::size_t _ruleLineNr;

        std::string _inputAspectsName;
        std::vector<std::shared_ptr<Node>> _cmdInputs;
        std::vector<std::shared_ptr<Node>> _orderOnlyInputs;
        // _inputProducers contains pointers to CommandNode and/or GroupNode
        std::unordered_set<std::shared_ptr<Node>> _inputProducers;
        std::string _script;
        std::weak_ptr<DirectoryNode> _workingDir;
        std::vector<std::shared_ptr<GeneratedFileNode>> _mandatoryOutputs;
        std::vector<std::filesystem::path> _optionalOutputs;
        std::vector<std::filesystem::path> _ignoreOutputs;
        std::vector<Glob> _optionalOutputGlobs;
        std::vector<Glob> _ignoreOutputGlobs;

        std::atomic<std::shared_ptr<IMonitoredProcess>> _scriptExecutor;
        std::shared_ptr<PostProcessor> _postProcessor;
        
        // Optional outputs detected during last script execution. 
        OutputNodes _detectedOptionalOutputs;

        // Union of _mandatoryOutputs and _detectedOptionalOutputs
        OutputNodes _detectedOutputs;

        // Inputs detected during last script execution.
        InputNodes _detectedInputs;

        // The hash of the hashes of all items that, when changed, invalidate
        // the output files. Items include the script, the output files and 
        // the relevant aspects of the input files.
        XXH64_hash_t _executionHash;
    };
}
