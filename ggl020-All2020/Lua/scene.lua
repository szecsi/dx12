keysPressed = {
	U=false,
	J=false,
	H=false,
	K=false,
	VK_SPACE   = false,
	VK_NUMPAD0 = false,
	VK_NUMPAD1 = false,
	VK_NUMPAD2 = false,
	VK_NUMPAD3 = false,
	VK_NUMPAD4 = false,
	VK_NUMPAD5 = false,
	VK_NUMPAD6 = false,
	VK_NUMPAD7 = false,
	VK_NUMPAD8 = false,
	VK_NUMPAD9 = false
}

dt = 0.1

multiMeshes = {}
geometries = {}
entities = {}
materials = {}
shaders = {}
physicsMaterials = {}
physicsModels = {}
cameras = {}


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


multiMeshes.giraffe = O:MultiMeshFromFile(_, {file="geopod.x"})
--entities.giraffe = O:StaticEntity(_, {multiMesh=multiMeshes.giraffe} )


krumpli = O:Shader(_, {file="shaders/texturedVS.cso"})
shaders.ps = O:Shader(_, {file="shaders/texturedPS.cso"})

materials.burned = O:Material(_, { vs=krumpli, ps=shaders.ps}, function(_)
	O:setTexture2D(_, {file="burned.jpg"})
	O:setTextureCube(_, {file="cloudyNoon.dds"})
end)

materials.metal = O:Material(_, { vs=krumpli, ps=shaders.ps}, function(_)
	O:setTexture2D(_, {file="ggmetal.png"})
	O:setTextureCube(_, {file="cloudyNoon.dds"})
end)

multiMeshes.burnedGiraffe = O:MultiMesh(_, {}, function(multiMeshToPopulateWithFlippedMeshes)
	O:FlipMesh(multiMeshToPopulateWithFlippedMeshes, {}, function(_)
		O:ShadedMesh(_, { mien=0, geometry=multiMeshes.giraffe:getGeometry(0, 0), material=materials.burned})
	end)
	O:FlipMesh(multiMeshToPopulateWithFlippedMeshes, {}, function(_)
		O:ShadedMesh(_, { mien=0, geometry=multiMeshes.giraffe:getGeometry(0, 1), material=materials.metal})
	end)
end)

--entities.burnedGiraffe = O:StaticEntity(_, {multiMesh=multiMeshes.burnedGiraffe, position={x=10, y=0, z=10} } )


geometries.quad = O:IndexedGeometry(_, {file="quad.x"})

shaders.quadVS = O:Shader(_, {file="shaders/quadVS.cso"})
shaders.backgroundPS = O:Shader(_, {file="shaders/backgroundPS.cso"})

materials.env = O:Material(_, { vs=shaders.quadVS, ps=shaders.backgroundPS, usePerObjectData=false, useDepthTest=false}, function(_)
	O:setTextureCube(_, {file="cloudyNoon.dds"})
end)

multiMeshes.env = O:MultiMesh(_, {}, function(_)
	O:FlipMesh(_, {}, function(_)
		O:ShadedMesh(_, { mien=0, geometry=geometries.quad, material=materials.env})
	end)
end)

entities.env = O:StaticEntity(_, {multiMesh=multiMeshes.env } )

physicsMaterials.rubber = O:PhysicsMaterial(_, {restitution=0.5})

function geopodShapes(_)
	O:Shape(_, {geometryType='eCAPSULE', material=physicsMaterials.rubber, orientationAngle=0, orientationAxis={x=0, y=1, z=0}, halfHeight=5, radius=3, position = { y=0 } } )
	O:Shape(_, {geometryType='eBOX',     material=physicsMaterials.rubber, orientationAngle=0.79, orientationAxis={x=0, y=1, z=0}, halfExtents = {x=4, y=1, z=4}, position = { x=4.5, y=-1, z=0 } } )
	O:Shape(_, {geometryType='eBOX',     material=physicsMaterials.rubber, orientationAngle=0.79, orientationAxis={x=0, y=1, z=0}, halfExtents = {x=4, y=1, z=4}, position = { x=-5, y=-1, z=-2 } } )
	O:Shape(_, {geometryType='eBOX',     material=physicsMaterials.rubber, orientationAngle=0.79, orientationAxis={x=0, y=1, z=0}, halfExtents = {x=4, y=1, z=4}, position = { x=-5, y=-1, z=2 } } )
	O:Shape(_, {geometryType='eBOX',     material=physicsMaterials.rubber, orientationAngle=1.098, orientationAxis={x=-0.146, y=-0.353, z=-0.353}, halfExtents = {x=4, y=1, z=4}, position = { x=-7, y=2, z=0 } } )
end

physicsModels.pod = O:PhysicsModel(_, {actorFlags={"eDISABLE_GRAVITY"}}, geopodShapes --[[function( _ )
	O:Shape(_, {
		material=physicsMaterials.rubber,
		geometryType="eSPHERE",
		radius=1} )
end --]]
)

physicsModels.kpod = O:PhysicsModel(_, {rigidBodyFlags={"eKINEMATIC"}}, geopodShapes --[[function( _ )
	O:Shape(_, {
		material=physicsMaterials.rubber,
		geometryType="eSPHERE",
		radius=1} )
end--]]
)

multiMeshes.rocket = O:MultiMeshFromFile(_, {file='rocket.x'})

physicsModels.rocket = O:PhysicsModel(_, { actorFlags={"eDISABLE_GRAVITY"} }, function(_)
	O:Shape(_, {geometryType='eCAPSULE', material=physicsMaterials.rubber, orientationAngle=0, orientationAxis={x=0, y=1, z=0}, halfHeight=2, radius=1, position = { y=0 } } )
end)

rocketControlState = { 
	script = function(entity, state) end,
	onContact = function(entity, state, otherState)
		state.killed = true
	end
}



playerControlState = {
	cooldown = 0.5,
	cooldownRemaining = 0,
	script = function(entity, controlState)
		controlState.cooldownRemaining = controlState.cooldownRemaining - dt

		if keysPressed.VK_NUMPAD5 == true then O:addForceAndTorque(entity, { force={x= 100000} } ) end
		if keysPressed.VK_NUMPAD0 == true then 
			O:addForceAndTorque(entity, { force={x=-100000}} )
			--controlState.killed = true
			end
		if keysPressed.VK_NUMPAD6 == true then O:addForceAndTorque(entity, { torque={y= 100000} } ) end
		if keysPressed.VK_NUMPAD4 == true then O:addForceAndTorque(entity, { torque={y=-100000}} ) end
		if keysPressed.VK_NUMPAD8 == true then O:addForceAndTorque(entity, { torque={z= 100000} } ) end
		if keysPressed.VK_NUMPAD2 == true then O:addForceAndTorque(entity, { torque={z=-100000}} ) end
		if keysPressed.VK_NUMPAD7 == true then O:addForceAndTorque(entity, { torque={x= 100000} } ) end
		if keysPressed.VK_NUMPAD9 == true then O:addForceAndTorque(entity, { torque={x=-100000}} ) end
		if keysPressed.VK_SPACE == true and (controlState.cooldownRemaining <= 0) then 
			controlState.cooldownRemaining = controlState.cooldown
			O:spawn(entity, {
				controlState=clone(rocketControlState),
				multiMesh=multiMeshes.rocket,
				position={ x=15, y=-2 } ,
				linearVelocity={x=30},
				model=physicsModels.rocket,
				linearDamping=1,
				angularDamping=1}) 
		end

	end
}

entities.avatar = O:ControlledEntity(_, {
	controlState = playerControlState,
	multiMesh=multiMeshes.giraffe,
	model=physicsModels.pod,
	position={},
	linearDamping=1,
	angularDamping=1
	})

O:PhysicsEntity(_, {
	multiMesh=multiMeshes.giraffe,
	model=physicsModels.kpod,
	position={y=-20}})

cameras.fixed = O:FixedCam(_, {
	owner=entities.avatar,
	position={x=-15, y=10}
})


interceptorControlState = {
	script = function(entity, controlState)
		O:addForceAndTorqueForTarget(entity, {
			mark=entities.avatar,
			position={y=0, z=0 },
			maxForce=10000,
			maxTorque=10000
			})
	end
}

cruiserControlState = {
	checkpoints = {
		{x = -150, y=50},
		{z = -150, y=50},
		{x = 150, y=50},
		{z = 150, y=50}
	},
	checkpointIndex = 2,
	script = function(entity, controlState)
		targetReached = O:addForceAndTorqueForTarget(entity, {
			position=controlState.checkpoints[controlState.checkpointIndex],
			maxForce=40000,
			maxTorque=20000,
			proximityRadius=10
			})
	if targetReached then
			controlState.checkpointIndex = controlState.checkpointIndex + 1
			if controlState.checkpointIndex > #(controlState.checkpoints) then
				controlState.checkpointIndex = 1
			end
		end
	end
}

O:ControlledEntity(_, {
	controlState = clone(cruiserControlState),
	multiMesh=multiMeshes.giraffe,
	model=physicsModels.pod,
	position={y=20},
	linearDamping=1,
	angularDamping=1
	})

O:ControlledEntity(_, {
	controlState = clone(cruiserControlState),
	multiMesh=multiMeshes.giraffe,
	model=physicsModels.pod,
	position={y=20, x=400},
	linearDamping=1,
	angularDamping=1
	})


geometries.torus = O:IndexedGeometryWithTangentSpace(_, {file="torusNiceUV.obj"})

shaders.normalMapVS = O:Shader(_, {file="shaders/NormalMapVS.cso"})
shaders.normalMapPS = O:Shader(_, {file="shaders/NormalMapPS.cso"})
shaders.parallaxPS = O:Shader(_, {file="shaders/ParallaxPS.cso"})
shaders.olparallaxPS = O:Shader(_, {file="shaders/OLParallaxPS.cso"})
shaders.binaryReliefPS = O:Shader(_, {file="shaders/BinaryReliefInsetPS.cso"})

materials.normalMapped = O:Material(_, { vs=shaders.normalMapVS, ps=shaders.normalMapPS}, function(_)
--	O:setTexture2D(_, {file="rkd.jpg"})
--	O:setTexture2D(_, {file="rnormal.jpg"})
--	O:setTexture2D(_, {file="rbump.jpg"})
	O:setTexture2D(_, {file="sponge-diffuse.jpg"})
	O:setTexture2D(_, {file="sponge-normal.jpg"})
	O:setTexture2D(_, {file="sponge-height.jpg"})
	O:setTextureCube(_, {file="cloudyNoon.dds"})
end)

materials.parallaxMapped = O:Material(_, { vs=shaders.normalMapVS, ps=shaders.parallaxPS}, function(_)
--	O:setTexture2D(_, {file="rkd.jpg"})
--	O:setTexture2D(_, {file="rnormal.jpg"})
--	O:setTexture2D(_, {file="rbump.jpg"})
	O:setTexture2D(_, {file="sponge-diffuse.jpg"})
	O:setTexture2D(_, {file="sponge-normal.jpg"})
	O:setTexture2D(_, {file="sponge-height.jpg"})
	O:setTextureCube(_, {file="cloudyNoon.dds"})
	O:setTextureCube(_, {file="cloudyNoon.dds"})
end)

materials.olparallaxMapped = O:Material(_, { vs=shaders.normalMapVS, ps=shaders.olparallaxPS}, function(_)
--	O:setTexture2D(_, {file="rkd.jpg"})
--	O:setTexture2D(_, {file="rnormal.jpg"})
--	O:setTexture2D(_, {file="rbump.jpg"})
	O:setTexture2D(_, {file="sponge-diffuse.jpg"})
	O:setTexture2D(_, {file="sponge-normal.jpg"})
	O:setTexture2D(_, {file="sponge-height.jpg"})
	O:setTextureCube(_, {file="cloudyNoon.dds"})	O:setTextureCube(_, {file="cloudyNoon.dds"})
end)

materials.binaryReliefMapped = O:Material(_, { vs=shaders.normalMapVS, ps=shaders.binaryReliefPS}, function(_)
--	O:setTexture2D(_, {file="rkd.jpg"})
--	O:setTexture2D(_, {file="rnormal.jpg"})
--	O:setTexture2D(_, {file="rbump.jpg"})
	O:setTexture2D(_, {file="sponge-diffuse.jpg"})
	O:setTexture2D(_, {file="sponge-normal.jpg"})
	O:setTexture2D(_, {file="sponge-height.jpg"})
	O:setTextureCube(_, {file="cloudyNoon.dds"})
	O:setTextureCube(_, {file="cloudyNoon.dds"})
end)

multiMeshes.normalMappedTorus = O:MultiMesh(_, {}, function(_)
	O:FlipMesh(_, {}, function(_)
		O:ShadedMesh(_, { mien=0, geometry=geometries.torus, material=materials.normalMapped})
	end)
end)

multiMeshes.parallaxMappedTorus = O:MultiMesh(_, {}, function(_)
	O:FlipMesh(_, {}, function(_)
		O:ShadedMesh(_, { mien=0, geometry=geometries.torus, material=materials.parallaxMapped})
	end)
end)

multiMeshes.olparallaxMappedTorus = O:MultiMesh(_, {}, function(_)
	O:FlipMesh(_, {}, function(_)
		O:ShadedMesh(_, { mien=0, geometry=geometries.torus, material=materials.olparallaxMapped})
	end)
end)

multiMeshes.binaryReliefMappedTorus = O:MultiMesh(_, {}, function(_)
	O:FlipMesh(_, {}, function(_)
		O:ShadedMesh(_, { mien=0, geometry=geometries.torus, material=materials.binaryReliefMapped})
	end)
end)

entities.normalMappedTorus0 = O:StaticEntity(_, {multiMesh=multiMeshes.normalMappedTorus, position={x=100, y=0, z=10} } )
entities.parallaxMappedTorus0 = O:StaticEntity(_, {multiMesh=multiMeshes.parallaxMappedTorus, position={x=100, y=0, z=-10} } )
entities.olparallaxMappedTorus0 = O:StaticEntity(_, {multiMesh=multiMeshes.olparallaxMappedTorus, position={x=100, y=0, z=-30} } )
entities.binaryReliefMappedTorus0 = O:StaticEntity(_, {multiMesh=multiMeshes.binaryReliefMappedTorus, position={x=100, y=0, z=-50} } ) 