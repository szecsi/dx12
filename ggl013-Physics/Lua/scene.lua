multiMeshes = {}
geometries = {}
entities = {}
materials = {}
shaders = {}
pxMaterials = {}
pxModels = {}

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

-------------------

shaders.quadVs = O:Shader(_, {file="Shaders/quadVS.cso"})
shaders.bgPs = O:Shader(_, {file="Shaders/bgPS.cso"})

geometries.quad = O:IndexedGeometry(_, {file='quad.x'})

materials.background = O:Material(_, {vs=shaders.quadVs, ps=shaders.bgPs, usePerObjectData=false}, function(_)
  O:setTexture2D(_, {file='giraffe.jpg'})
  O:setTextureCube(_, {file='cloudynoon.dds'})
end )

multiMeshes.backgroundQuad = O:MultiMesh(_, {}, function(_)
  O:FlipMesh(_, {}, function(_)
    O:ShadedMesh(_, {mien=0, geometry=geometries.quad, material=materials.background})
  end )
end )

entities.background = O:StaticEntity(_, {multiMesh=multiMeshes.backgroundQuad } )

--------------

multiMeshes.patchPod = O:MultiMeshFromFile(_, {file='geopod.x', topology="patch"})
geometries.patchChassis = multiMeshes.patchPod:getGeometry(0, 0)
geometries.patchWindows = multiMeshes.patchPod:getGeometry(0, 1)

shaders.tessVs = O:Shader(_, {file="Shaders/TessVS.cso"})
shaders.tessHs = O:Shader(_, {file="Shaders/TessHS.cso"})
shaders.tessDs = O:Shader(_, {file="Shaders/TessDS.cso"})

materials.tess = O:Material(_, {
	wireframe=true,
	vs=shaders.tessVs,
	hs=shaders.tessHs,
	ds=shaders.tessDs,
	ps=shaders.ps}, function(_)
  O:setTexture2D(_, {file='giraffe.jpg'})
  O:setTextureCube(_, {file='cloudynoon.dds'})
end )

multiMeshes.tessPod = O:MultiMesh(_, {}, function(_)
  O:FlipMesh(_, {}, function(_)
    O:ShadedMesh(_, {mien=0, geometry=geometries.patchChassis, material=materials.tess})
  end )
  O:FlipMesh(_, {}, function(_)
    O:ShadedMesh(_, {mien=0, geometry=geometries.patchWindows, material=materials.tess})
  end )
end )

entities.tessPod = O:StaticEntity(_, {multiMesh=multiMeshes.tessPod, position = { x=-20, y=-10, z=0} } )

---------------
--[[
materials.tessQuad = O:Material(_, {
	wireframe=true,
	vs=shaders.tessVs,
	hs=shaders.tessQuadHs,
	ds=shaders.tessQuadDs,
	ps=shaders.ps}, function(_)
  O:setTexture2D(_, {file='giraffe.jpg'})
  O:setTextureCube(_, {file='cloudynoon.dds'})
end )

multiMeshes.y = O:MultiMeshFromFile(_, {file='YbranchLow.obj', topology="patch4", flags={}})
geometries.y = multiMeshes.y:getGeometry(0, 0)

multiMeshes.tessPod = O:MultiMesh(_, {}, function(_)
  O:FlipMesh(_, {}, function(_)
    O:ShadedMesh(_, {mien=0, geometry=geometries.y, material=materials.tessQuad})
  end )
end )

entities.y = O:StaticEntity(_, {multiMesh=multiMeshes.tessPod, position = { x=0, y=30, z=0} } )
--]]


pxMaterials.default = O:PhysicsMaterial(_, {})

function geopodShapes(_)
	O:Shape(_, {geometryType='eCAPSULE', material=pxMaterials.default, orientationAngle=0, orientationAxis={x=0, y=1, z=0}, halfHeight=5, radius=3, position = { y=0 } } )
	O:Shape(_, {geometryType='eBOX',     material=pxMaterials.default, orientationAngle=0.79, orientationAxis={x=0, y=1, z=0}, halfExtents = {x=4, y=1, z=4}, position = { x=4.5, y=-1, z=0 } } )
	O:Shape(_, {geometryType='eBOX',     material=pxMaterials.default, orientationAngle=0.79, orientationAxis={x=0, y=1, z=0}, halfExtents = {x=4, y=1, z=4}, position = { x=-5, y=-1, z=-2 } } )
	O:Shape(_, {geometryType='eBOX',     material=pxMaterials.default, orientationAngle=0.79, orientationAxis={x=0, y=1, z=0}, halfExtents = {x=4, y=1, z=4}, position = { x=-5, y=-1, z=2 } } )
	O:Shape(_, {geometryType='eBOX',     material=pxMaterials.default, orientationAngle=1.098, orientationAxis={x=-0.146, y=-0.353, z=-0.353}, halfExtents = {x=4, y=1, z=4}, position = { x=-7, y=2, z=0 } } )
end

pxModels.geopodKin = O:PhysicsModel(_, { rigidBodyFlags={"eKINEMATIC"}}, geopodShapes)
pxModels.geopod = O:PhysicsModel(_, {  }, geopodShapes)

O:PhysicsEntity(_, {model=pxModels.geopod, multiMesh=multiMeshes.pod, position={y=30}, linearVelocity={y=10}, angularVelocity={y=2}} )
O:PhysicsEntity(_, {model=pxModels.geopodKin, multiMesh=multiMeshes.pod, linearVelocity={y=10}, angularVelocity={y=2}} )
