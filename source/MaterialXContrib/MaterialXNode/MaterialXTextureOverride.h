#ifndef MaterialXTextureOverride_H
#define MaterialXTextureOverride_H

#include <maya/MPxShadingNodeOverride.h>

/// VP2 Texture fragment override
class MaterialXTextureOverride : public MHWRender::MPxShadingNodeOverride
{
  public:
	static MHWRender::MPxShadingNodeOverride* creator(const MObject& obj);

	~MaterialXTextureOverride() override;

	MHWRender::DrawAPI supportedDrawAPIs() const override;

	MString fragmentName() const override;
	void getCustomMappings(MHWRender::MAttributeParameterMappingList& mappings) override;

	void updateDG() override;
	void updateShader(MHWRender::MShaderInstance& shader,
	                  const MHWRender::MAttributeParameterMappingList& mappings) override;

  private:
	MaterialXTextureOverride(const MObject& obj);

	MString _document;
	MString _element;
	MObject _object;
};

#endif /* MATERIALX_NODE_OVERRIDE_H */
