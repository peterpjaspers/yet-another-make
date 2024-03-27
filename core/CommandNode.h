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

        // Output filters define how CommandNode treats detected output files.
        // A CommandNode can have 0, 1 or more output filters.
        // 
        // Output and ExtraOutput: _path is a path relative to the CommandNode
        // working directory. All Output and  ExtraOutput files must be in the
        // set of detected output files, execution fails otherwise. 
        // ExtraOutputs are treated like Outputs except that they do not appear
        // in the %o flag(s) in the command script.
        // Detected files that do not match Output or ExtraOutput filters must 
        // match Optional or Ignore filters, execution fails otherwise.
        // 
        // Optional and Ignore: _path is a path or a glob path relative to the
        // CommandNode working directory. A glob path is a path with glob 
        // special characters, see class Glob.
        // Files that match an Optional filter are accepted as valid output and
        // are registered as output dependency.
        // Files that match an Ignore filter are deleted and not registered as
        // output dependency.
        // 
        // Filter precedence: the Output and ExtraOutput filters have precedence
        // over Optional and Ignore filters: an Ignore or optional filter that 
        // matches a file that also matches an Output or ExtraOutput file has 
        // no effect.
        // The order of Optional and Ignore filters defines precedence. E.g. 
        // Optional * followed by Ignore *.txt accept as outputs all files 
        // except .txt files. E.g. Ignore *.txt followed by Optional foo.txt 
        // ignores all .txt files except foo.txt. 
        //
        struct OutputFilter {
            enum Type { Output, ExtraOutput, Optional, Ignore, None };
            OutputFilter() {}
            OutputFilter(Type type, std::filesystem::path const& path);
            Type _type;
            std::filesystem::path _path;
            bool operator==(OutputFilter const& rhs) const;
            void stream(IStreamer* streamer);
            static void streamVector(IStreamer* streamer, std::vector<OutputFilter>& filters);
        };

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

        // Set/get output filter and output nodes for the paths specified in
        // filters of type Output and ExtraOutput.
        void outputFilters(
            std::vector<OutputFilter> const& newFilters,
            std::vector<std::shared_ptr<GeneratedFileNode>> const &mandatoryOutputs);
        std::vector<OutputFilter> const& outputFilters() const;

        // Convenience function in case command only has mandatory outputs.
        // Equivalent to: outputFilters(mandatoryFiltersForOutputs, outputs).
        void mandatoryOutputs(std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs);

        // Return the output nodes passed to outputFilters(..) of type Output 
        // and ExtraOutput.
        OutputNodes const& mandatoryOutputs() const;
        std::vector<std::shared_ptr<GeneratedFileNode>> mandatoryOutputsVec() const;

        // Return optional outputs detected during last script execution.
        // Command node owns these nodes, i.e. is responsible for adding/removing
        // them from execution context.
        // Pre: state() == Node::State::Ok
        OutputNodes const& detectedOptionalOutputs() const;

        // Return union of mandatoryOutputs() and detectedOptionalOutputs() 
        // Pre: state() == Node::State::Ok
        std::vector<std::shared_ptr<GeneratedFileNode>> detectedOutputs() const;

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
        void start(PriorityClass prio) override;
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
        struct OutputNameFilter {
            OutputNameFilter(OutputFilter::Type type, std::filesystem::path const& symPath);
            OutputFilter::Type _type;
            std::filesystem::path _symPath; // Symbolic path for workingDir()/_filter.path
            Glob _glob; // Glob(_symPath);
        };

        struct ExecutionResult {
            MemoryLogBook _log;
            Node::State _newState;
            std::map<std::filesystem::path, OutputFilter::Type> _outputPaths;
            std::set<std::filesystem::path> _keptInputPaths;
            std::set<std::filesystem::path> _removedInputPaths;
            std::set<std::filesystem::path> _addedInputPaths;
            std::vector<std::shared_ptr<FileNode>> _addedInputNodes;
        };
        void updateOutputNameFilters();
        void updateMandatoryOutputs(std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs);
        void clearOptionalOutputs();
        OutputFilter::Type findFilterType(
            std::filesystem::path const& symPath,
            std::vector<OutputNameFilter> const& filters) const;
        std::vector<std::shared_ptr<GeneratedFileNode>> filterOutputs(
            OutputNodes const& outputs,
            CommandNode::OutputFilter::Type filterType);

        std::filesystem::path convertToSymbolicPath(std::filesystem::path const& absPath, MemoryLogBook& logBook);
        std::set<std::filesystem::path> convertToSymbolicPaths(
            std::set<std::filesystem::path> const& absPaths,
            MemoryLogBook& logBook);
        void handleRequisitesCompletion(Node::State state);
        void executeScript();
        void handleExecuteScriptCompletion(std::shared_ptr<ExecutionResult> result);
        void handleOutputAndNewInputFilesCompletion(Node::State newState, std::shared_ptr<ExecutionResult> result);
        std::vector<OutputNameFilter> const& outputNameFilters();
        void updateInputProducers();

        std::string compileScript(ILogBook& logBook);
        MonitoredProcessResult executeMonitoredScript(ILogBook& logBook);
        void getSourceInputs(std::vector<Node*>& sourceInputs) const;
        void clearDetectedInputs();
        void setDetectedInputs(ExecutionResult const& result);
        void setDetectedOptionalOutputs(
            std::vector<std::shared_ptr<GeneratedFileNode>> const& optionals,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& newOptionals);

        XXH64_hash_t computeExecutionHash(std::vector<OutputNameFilter> const& filters) const;

        bool findInputNodes(
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& allowedGenInputFiles,
            std::set<std::filesystem::path>const& inputSymPaths,
            std::vector<std::shared_ptr<FileNode>>& inputNodes,
            std::vector<std::shared_ptr<Node>>& srcInputNodes,
            ILogBook& logBook
        );
        bool findOutputNodes(
            std::map<std::filesystem::path, OutputFilter::Type> const& outputSymPaths,
            std::vector<std::shared_ptr<GeneratedFileNode>>& optionalOutputNodes,
            std::vector<std::shared_ptr<GeneratedFileNode>>& newOptionalOutputNodes,
            MemoryLogBook& logBook);
        bool verifyMandatoryOutputs(
            std::vector<std::shared_ptr<GeneratedFileNode>> const& outputNodes,
            MemoryLogBook& logBook);


        // buildFile that contains the rule from which this command was created.
        SourceFileNode* _buildFile;
        std::size_t _ruleLineNr;

        std::string _inputAspectsName;
        std::vector<std::shared_ptr<Node>> _cmdInputs;
        std::vector<std::shared_ptr<Node>> _orderOnlyInputs;
        std::weak_ptr<DirectoryNode> _workingDir;
        std::string _script;
        std::shared_ptr<PostProcessor> _postProcessor;
        std::vector<OutputFilter> _outputFilters;

        // _inputProducers contains command and/or group nodes
        // present in _cmdInputs and _orderOnlyInputs
        std::unordered_set<std::shared_ptr<Node>> _inputProducers;

        std::vector<OutputNameFilter> _outputNameFilters;

        std::atomic<std::shared_ptr<IMonitoredProcess>> _scriptExecutor;

        // Mandatory and extra-mandatory outputs
        OutputNodes _mandatoryOutputs;

        // Optional outputs detected during last script execution.
        OutputNodes _detectedOptionalOutputs;

        // Inputs detected during last script execution.
        InputNodes _detectedInputs;

        // The hash of the hashes of all items that, when changed, invalidate
        // the output files.
        XXH64_hash_t _executionHash;
    };
}
