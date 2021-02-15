-- Global array with keys used in the script. PhysicsApp::animate will update these, and only these, in every frame.
keysPressed={
	U=false,
	J=false,
	H=false,
	K=false,
	VK_SPACE=false,
	VK_NUMPAD0=false,
	VK_NUMPAD1=false,
	VK_NUMPAD2=false,
	VK_NUMPAD3=false,
	VK_NUMPAD4=false,
	VK_NUMPAD5=false,
	VK_NUMPAD6=false,
	VK_NUMPAD7=false,
	VK_NUMPAD8=false,
	VK_NUMPAD9=false
}
dt = 0.1

cloneInstanceId = 0
-- Function to perform a one level deep copy on a table. Useful to clone control state tables.
function clone(t)
  local t2 = {}
  for k,v in pairs(t) do
    t2[k] = v
  end
  t2.instanceId = cloneInstanceId
  cloneInstanceId = cloneInstanceId + 1
  return t2
end

------------------------------------------------------------

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
--O:StaticEntity(_, {multiMesh=multiMeshes.pod,
--            position = { x=0, y=-10, z=0} } )


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

--entities.pod2 = O:StaticEntity(_, {multiMesh=multiMeshes.pod2, position = { x=20, y=-10, z=0} } )

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

--entities.tessPod = O:StaticEntity(_, {multiMesh=multiMeshes.tessPod, position = { x=-20, y=-10, z=0} } )

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
pxModels.geopod = O:PhysicsModel(_, { }, geopodShapes)
pxModels.geopodHover = O:PhysicsModel(_, { actorFlags={"eDISABLE_GRAVITY"} }, geopodShapes)

--O:PhysicsEntity(_, {model=pxModels.geopod, multiMesh=multiMeshes.pod, position={y=30}, linearVelocity={y=10}, angularVelocity={y=2}} )
--O:PhysicsEntity(_, {model=pxModels.geopodKin, multiMesh=multiMeshes.pod, linearVelocity={y=10}, position={x=30}, angularVelocity={y=2}} )

-- A lua table that can be set as the controlState object of a PhysicsEntity. It must have a 'script' key, with a function value. PhysicsEntity::animate will call it.
playerControlState = { 
	cooldown = 0.5,
	cooldownRemaining = 0,
	script = function(entity, state)
		state.cooldownRemaining = state.cooldownRemaining - dt
		if keysPressed.VK_NUMPAD5 == true then O:addForceAndTorque(entity, { force={x= 100000} } ) end
		if keysPressed.VK_NUMPAD0 == true then O:addForceAndTorque(entity, { force={x=-100000}} ) end
		if keysPressed.VK_NUMPAD6 == true then O:addForceAndTorque(entity, { torque={y= 100000} } ) end
		if keysPressed.VK_NUMPAD4 == true then O:addForceAndTorque(entity, { torque={y=-100000}} ) end
		if keysPressed.VK_NUMPAD8 == true then O:addForceAndTorque(entity, { torque={z= 100000} } ) end
		if keysPressed.VK_NUMPAD2 == true then O:addForceAndTorque(entity, { torque={z=-100000}} ) end
		if keysPressed.VK_NUMPAD7 == true then O:addForceAndTorque(entity, { torque={x= 100000} } ) end
		if keysPressed.VK_NUMPAD9 == true then O:addForceAndTorque(entity, { torque={x=-100000}} ) end
		if (keysPressed.VK_SPACE  == true) and (state.cooldownRemaining <= 0) then 
			state.cooldownRemaining = state.cooldown
			O:spawn(entity, {
				controlState=clone(state.rocketControlState),
				multiMesh=multiMeshes.rocket,
				model=pxModels.rocket,
				position={ x=15, y=-2 },
				linearVelocity= {x=100},
				linearDamping=0.01, angularDamping=1 }) 
		end
	end,
	rocketControlState = { 
		script = function(entity, state) end,
		onContact = function(entity, state)
			state.killed = true
		end
	}
}

entities.avatar = O:ControlledEntity(_, {model=pxModels.geopodHover, position={y=0}, 
	controlState=playerControlState, multiMesh=multiMeshes.pod, 
	linearDamping=1, angularDamping=1 } )

O:FixedCam(_, {position={x=-15, y=15}, owner=entities.avatar})


-------------------------------------------

-- A lua table that can be set as the controlState object of a PhysicsEntity. It must have a 'script' key, with a function value. PhysicsEntity::animate will call it.
-- This controlState has additional data. The use of additional state data is at the script-writer's discretion.
-- Note that it is not a good idea to pass the same controlState to multiple PhysicsEntities. Use the clone function to get copies.
cruiserControlState = { 
	targetIndex = 1,
	targets = { 
		{position={x=100, y=100}, proximityRadius=10, maxForce = 300000, maxTorque = 30000},
		{position={z=100, y=100}, proximityRadius=10, maxForce = 300000, maxTorque = 30000},		 
		{position={x=-100, y=100}, proximityRadius=10, maxForce = 300000, maxTorque = 30000},		 
		{position={z=-100, y=100}, proximityRadius=10, maxForce = 300000, maxTorque = 30000} },
	script = function(entity, state)
		if O:addForceAndTorqueForTarget(entity, state.targets[state.targetIndex] ) then
			state.targetIndex = state.targetIndex + 1
			if state.targetIndex > #(state.targets) then
				state.targetIndex = 1
			end
		end
	end
}

--O:ControlledEntity(_, {model=pxModels.geopodHover, position={y=60}, 
--	controlState=clone(cruiserControlState), multiMesh=multiMeshes.pod, 
--	linearDamping=1, angularDamping=1 } )

-----------------------------------------------
multiMeshes.rocket = O:MultiMeshFromFile(_, {file='rocket.x'})

pxModels.rocket = O:PhysicsModel(_, { actorFlags={"eDISABLE_GRAVITY"} }, function(_)
	O:Shape(_, {geometryType='eCAPSULE', material=pxMaterials.default, orientationAngle=0, orientationAxis={x=0, y=1, z=0}, halfHeight=2, radius=1, position = { y=0 } } )
end
)



