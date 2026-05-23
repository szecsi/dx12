nLevels = 1
nBranchTypes = 1
nTreeModels = 32 * 27
nInstancesPerTreeModel = 8 * 9
nMinPiecesPerTree = 33
nMaxPiecesPerTree = 127
nAveragePiecesPerTree = (nMinPiecesPerTree + nMaxPiecesPerTree) / 2

multiMeshes = {}
geometries = {}
entities = {}
materials = {}
shaders = {}

shaders.treeVS = O:Shader(_, {file="treeVS.cso"})
shaders.treePS = O:Shader(_, {file="treePS.cso"})

geometries.y2 = O:IndexedGeometryWithTangentSpaceAndRigging(_, {file="tree/y0.fbx",
    instanceCount= 32*27*256
    --nTreeModels*nInstancesPerTreeModel*nAveragePiecesPerTree
    })

materials.tree = O:Material(_, {rootParameterIndex=3, vs=shaders.treeVS, ps=shaders.treePS}, function(_)
  addTreeBuffers()
  O:setTexture2D(_, {file='tree/bark-alpha-tiling.png'})
end )

O:addGuiMaterial("TreeMaterialCb", materials.tree)

multiMeshes.tree = O:MultiMesh(_, {}, function(_)
  O:FlipMesh(_, {}, function(_)
    O:ShadedMesh(_, {mien=0, geometry=geometries.y2, material=materials.tree})
  end )
end )

entities.forest = O:StaticEntity(_, {multiMesh=multiMeshes.tree, position = { x=0, y=0, z=0} } )
