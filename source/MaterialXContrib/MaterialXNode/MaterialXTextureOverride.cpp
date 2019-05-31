#include "MaterialXTextureOverride.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MFragmentManager.h>
#include <maya/MRenderUtil.h>
#include <maya/MShaderManager.h>
#include <maya/MTextureManager.h>

MHWRender::MPxShadingNodeOverride* MaterialXTextureOverride::creator(const MObject& obj)
{
	return new MaterialXTextureOverride(obj);
}

MaterialXTextureOverride::MaterialXTextureOverride(const MObject& obj)
	: MPxShadingNodeOverride(obj)
	, _object(obj)
{
}

MaterialXTextureOverride::~MaterialXTextureOverride()
{
}

MHWRender::DrawAPI MaterialXTextureOverride::supportedDrawAPIs() const
{
	return MHWRender::kOpenGL | MHWRender::kOpenGLCoreProfile;
}

MString MaterialXTextureOverride::fragmentName() const
{
	return "TempName";
}

void MaterialXTextureOverride::getCustomMappings(MHWRender::MAttributeParameterMappingList& /*mappings*/)
{
}

void MaterialXTextureOverride::updateDG()
{
}

void MaterialXTextureOverride::updateShader(MHWRender::MShaderInstance& /*shader*/, 
                                            const MHWRender::MAttributeParameterMappingList& /*mappings*/)
{
}
