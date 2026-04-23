multiMeshes = {}
geometries = {}
entities = {}
materials = {}
shaders = {}

shaders.retamVs = O:Shader(_, {file="Shaders/Retam/retamVS.cso"})
shaders.retamCollectVs = O:Shader(_, {file="Shaders/Retam/retamCollectVS.cso"})
shaders.retam256Ps = O:Shader(_, {file="Shaders/Retam/retam256PS.cso"})
shaders.retam256CollectPs = O:Shader(_, {file="Shaders/Retam/retam256CollectPS.cso"})
shaders.layDownDepthPs = O:Shader(_, {file="Shaders/Retam/layDownDepthPS.cso"})

geometries.torus = O:IndexedGeometryWithTangentSpace(_, {file="torusNiceUV.obj"})

multiMeshes.pod = O:MultiMeshFromFile(_, {file='torusNiceUV.obj'})

geometries.chassis = multiMeshes.pod:getGeometry(0, 0)

materials.retam256 = O:Material(_, {rootParameterIndex=3, vs=shaders.retamVs, ps=shaders.retam256Ps}, function(_)
  O:setTexture2D(_, {file='carrot.jpg'})
end )

materials.layDownDepth = O:Material(_, {rootParameterIndex=3, vs=shaders.retamVs, ps=shaders.layDownDepthPs}, function(_)
  O:setTexture2D(_, {file='carrot.jpg'})
end )

materials.retam256Collect = O:Material(_, {rootParameterIndex=3, vs=shaders.retamCollectVs, ps=shaders.retam256CollectPs, useDepthTest=false}, function(_)
end )

O:addGuiMaterial("RetamMaterialCb", materials.retam256)
O:addGuiMaterial("retam256Collect", materials.retam256Collect)
O:addGuiMaterial("layDownDepth", materials.layDownDepth)

multiMeshes.podRetam256 = O:MultiMesh(_, {}, function(_)
  O:FlipMesh(_, {}, function(_)
    O:ShadedMesh(_, {mien=0, geometry=geometries.torus, material=materials.retam256})
    O:ShadedMesh(_, {mien=1, geometry=geometries.torus, material=materials.retam256Collect, renderTargets=0})
    O:ShadedMesh(_, {mien=2, geometry=geometries.torus, material=materials.layDownDepth, renderTargets=0})
  end )
end )

entities.podRetam256 = O:StaticEntity(_, {multiMesh=multiMeshes.podRetam256, position = { x=20, y=-10, z=0} } )
