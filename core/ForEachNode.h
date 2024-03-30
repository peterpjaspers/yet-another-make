#pragma once

#include "Node.h"
#include "BuildFile.h"
#include "xxhash.h"

#include <memory>
#include <vector>

namespace YAM
{
    class GroupNode;
    class DirectoryNode;
    class FileNode;
    class SourceFileNode;
    class CommandNode;

    // ForEachNode compiles a set of CommandNodes from a 'foreach' build
    // rule that contains input groups. This compilation cannot be done during
    // the buildfile compilation phase because group content is undefined until
    // the group has been executed.
    // 
    // The cmdInputs vector of a ForEachNode contains GeneratedFile and/or
    // Group nodes.
    // Definition:cmdInputFiles is the union of the files and the files in the
    // groups in cmdInputs.
    // 
    // A ForEachNode is created during the buildfile compilation phase when
    // compiling a 'foreach' rule that has input groups. The following input 
    // parameters are passed to the created ForEachNode:
    //     - cmdInputs: file and group nodes
    //     - order-only input: file and group nodes 
    //     - command script
    //     - output file specifications
    //
    // When a ForEachNode executes it:
    //     - creates a CommandNode for each file in cmdInputFiles and sets the
    //       following CommandNode properties:
    //         - cmdInputs, containing the single file from cmdInputFiles
    //         - working directory, same as ForEachNode working directory
    //         - OutputNameFilters, computed from the ForEachNode outputs(),
    //           with %-flags resolved from the single input file.
    //         - order-only inputs, same as ForEachNode order-only inputs
    //     - executes the CommandNodes
    // 
    // What if both child CommandNodes and ForEachNode become Dirty and, as a
    // consequence, are executed? This may cause a ForEachNode to delete a 
    // child node that already finished execution (which is a waste of time)
    // or, worse, while it is executing (causing havoc). This must be avoided 
    // by removing from the set of Dirty CommandNodes the nodes created by 
    // Dirty ForEachNodes.
    //
    class __declspec(dllexport) ForEachNode : public Node
    {
    public:

        ForEachNode(); // needed for deserialization
        ForEachNode(ExecutionContext* context, std::filesystem::path const& name);
        ~ForEachNode();

        std::string className() const override { return "ForEachNode"; }

        // Pre: newInputs contains SourceFileNode and/or GeneratedFileNode
        // and/or GroupNode instances.
        void cmdInputs(std::vector<std::shared_ptr<Node>> const& newInputs);
        std::vector<std::shared_ptr<Node>> const& cmdInputs() const;

        // Pre: newInputs contains GeneratedFileNode and/or GroupNode instances.
        void orderOnlyInputs(std::vector<std::shared_ptr<Node>> const& newInputs);
        std::vector<std::shared_ptr<Node>> const& orderOnlyInputs() const;

        // Return the groups in cmdInputs and orderOnlyInputs.
        std::vector<std::shared_ptr<GroupNode>> inputGroups() const;

        void script(std::string const& newScript);
        std::string const& script() const;

        // Set/get the output files
        void outputs(BuildFile::Outputs const& outputs);
        BuildFile::Outputs const& outputs() const;

        // The directory in which the script will be executed.
        // The repository root directory when nullptr.
        void workingDirectory(std::shared_ptr<DirectoryNode> const& dir);
        std::shared_ptr<DirectoryNode> workingDirectory() const;

        void buildFile(SourceFileNode* buildFile);
        void ruleLineNr(std::size_t ruleLineNr);
        SourceFileNode const* buildFile() const;
        std::size_t ruleLineNr() const;

        // Inherited from Node
        void start(PriorityClass prio) override;

        XXH64_hash_t executionHash() const;

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    private:
        void destroy(bool removeFromContext);
        void cleanup() override; 
        void updateInputGroups(); 
        void handleRequisitesCompletion(Node::State state);
        bool compileCommands();
        std::vector<std::shared_ptr<FileNode>> cmdInputFiles() const;
        std::shared_ptr<BuildFile::Rule> createRule(std::shared_ptr<FileNode> const& inputFile);
        void handleCommandsCompletion(Node::State newState);
        XXH64_hash_t computeExecutionHash() const;

        // Buildfile and rule from wich this node was created.
        SourceFileNode* _buildFile;
        std::size_t _ruleLineNr;

        std::vector<std::shared_ptr<Node>> _cmdInputs;
        std::vector<std::shared_ptr<Node>> _orderOnlyInputs;
        std::weak_ptr<DirectoryNode> _workingDir;
        std::string _script;
        BuildFile::Outputs _outputs;

        // the group nodes in _cmdInputs
        std::vector<std::shared_ptr<GroupNode>> _inputGroups;
        // the command nodes resulting from the foreach compilation.
        std::vector<std::shared_ptr<CommandNode>> _commands;
        // hash of the hashes of:
        //    - file names in _cmdInputs and _orderOnlyInputs
        //    - groups in _cmdInputs and _orderOnlyInputs
        //    - _script, 
        //    - _outputs 
        //    - _workingDir name
        XXH64_hash_t _executionHash;
    };
}


