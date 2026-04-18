multiMeshes = {}
geometries = {}
entities = {}
materials = {}
shaders = {}

shaders.retamVs = O:Shader(_, {file="Shaders/Retam/retamVS.cso"})
shaders.retam256Ps = O:Shader(_, {file="Shaders/Retam/retam256PS.cso"})

geometries.torus = O:IndexedGeometryWithTangentSpace(_, {file="torusNiceUV.obj"})

multiMeshes.pod = O:MultiMeshFromFile(_, {file='torusNiceUV.obj'})

geometries.chassis = multiMeshes.pod:getGeometry(0, 0)

materials.retam256 = O:Material(_, {rootParameterIndex=3, vs=shaders.retamVs, ps=shaders.retam256Ps}, function(_)
  O:setTexture2D(_, {file='carrot.jpg'})
end )

O:addGuiMaterial("RetamMaterialCb", materials.retam256)

multiMeshes.podRetam256 = O:MultiMesh(_, {}, function(_)
  O:FlipMesh(_, {}, function(_)
    O:ShadedMesh(_, {mien=0, geometry=geometries.torus, material=materials.retam256})
  end )
end )

entities.podRetam256 = O:StaticEntity(_, {multiMesh=multiMeshes.podRetam256, position = { x=20, y=-10, z=0} } )

