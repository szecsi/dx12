multiMeshes = {}
geometries = {}
entities = {}
materials = {}
shaders = {}

shaders.vs = O:Shader(_, {file="Shaders/trafoVS.cso"})
shaders.ps = O:Shader(_, {file="Shaders/MaxBlinnPS.cso"})
shaders.envmappedPs = O:Shader(_, {file="Shaders/EnvMapPS.cso"})

multiMeshes.pod = O:MultiMeshFromFile(_, {file='geopod.x'})
O:StaticEntity(_, {multiMesh=multiMeshes.pod,
            position = { x=0, y=-10, z=0} } )


-- LABTODO: Giraffe with manual MultiMesh

materials.spotted = O:Material(_, {vs=shaders.vs, ps=shaders.ps}, function(_)
  O:setTexture2D(_, {file='giraffe.jpg'})
  O:setTextureCube(_, {file='cloudynoon.dds'})
end )

-- LABTODO: Geopod with env mapped windows MultiMesh

geometries.chassis = multiMeshes.pod:getGeometry(0, 0)
geometries.windows = multiMeshes.pod:getGeometry(0, 1)

materials.envmapped = O:Material(_, {vs=shaders.vs, ps=shaders.envmappedPs}, function(_)
  O:setTexture2D(_, {file='giraffe.jpg'})
  O:setTextureCube(_, {file='cloudynoon.dds'})
end )

multiMeshes.pod2 = O:MultiMesh(_, {}, function(_)
  O:FlipMesh(_, {}, function(_)
    O:ShadedMesh(_, {mien=0, geometry=geometries.chassis, material=materials.spotted})
  end )
  O:FlipMesh(_, {}, function(_)
    O:ShadedMesh(_, {mien=0, geometry=geometries.windows, material=materials.envmapped})
  end )
end )



entities.pod2 = O:StaticEntity(_, {multiMesh=multiMeshes.pod2, position = { x=20, y=-10, z=0} } )

