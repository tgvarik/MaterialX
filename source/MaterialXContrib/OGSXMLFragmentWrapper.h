#ifndef MATERIALX_OGSX_ML_FRAGMENT_WRAPPER_H
#define MATERIALX_OGSX_ML_FRAGMENT_WRAPPER_H

/// @file
/// Contribution utility methods

#include <MaterialXGenShader/Library.h>

#include <MaterialXCore/Document.h>
#include <MaterialXCore/Element.h>

namespace MaterialX
{
/// @class OGSXMLFragmentWrapper
/// Class representing an OGS XML Fragment Wrapper
/// Interfaces allow the wrapper to be generated based on an input node and generator
/// The wrapper can contain fragment descriptions for more than one node if desired.
class OGSXMLFragmentWrapper
{
  public:
    /// Default constructor
    OGSXMLFragmentWrapper();

    /// Default desctructor
    ~OGSXMLFragmentWrapper();

    /// Add a fragment wrapper from a node definition
    void createWrapperFromNode(NodePtr node, std::vector<GenContext*> contexts);
    /// Add a fragment wrapper for a shader generated from a given node for a given context
    void createWrapperFromShader(NodePtr node, GenContext& context);

    /// Get the contents of the cached XML document as a stream.
    void getDocument(std::ostream& stream);

    /// Set to output vertex shader code (if any)
    void setOutputVertexShader(bool val)
    {
        _outputVertexShader = val;
    }

    /// Query if we want to output vertex shader code
    bool getOutputVertexShader() const
    {
        return _outputVertexShader;
    }

    /// Get list of Element paths and corresponding fragment input names
    const StringMap& getPathInputMap() const
    {
        return _pathInputMap;
    }

    /// Get list of Element paths and corresponding fragment output names
    const StringMap& getPathOutputMap() const
    {
        return _pathOutputMap;
    }

  protected:
    // Mapping from MTLX keywords to OGS fragment XML keywords
    StringMap _typeMap;

    bool _outputVertexShader = false;
    
    // Internally cache XML document
    void* _xmlDocument;

    // Mapping from MTLX Element paths to fragment input names
    StringMap _pathInputMap;

    // Mapping from MTLX Element paths to fragment output names
    StringMap _pathOutputMap;
};

} // namespace MaterialX

#endif
