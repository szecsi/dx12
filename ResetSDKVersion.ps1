# Which platform version are we building with?
# VS 2015: v140 (tested, compiles)
# VS 2017: v141 (tested, DOES NOT compile because of wrl/client.h include)
# VS 2019: v142 (tested, compiles)
$targetToolset = "v140"

# Which Windows SDK are we using?
# 10.0.14393.0     | is included in VS 2015 update 3 (tested)
# 10.0.15063.0     | is included in VS 2017 15.1 
# 10.0.16299.15    | is included in VS 2017 15.4
# 10.0.17134.0     | is included in VS 2017 15.7
# 10.0.17763.0     | is included in VS 2017 15.8
# 10.0.18362.0     | is included in VS 2019 (tested)
$targetSDK = "10.0.14393.0"

# helper function to query the existence of XML children node
function Has-Property {
    param(
        $Node,
        [string] $PropertyToQuery
    )

    Get-Member -inputobject $Node -name $PropertyToQuery -Membertype Properties
}

Get-ChildItem -Path .\ -Filter *.vcxproj -Recurse -File | ForEach-Object {
    $location = $_.FullName

    [xml]$doc = Get-Content $location

    foreach($propertyGroup in $doc.Project.PropertyGroup) {
        if(Has-Property -Node $propertyGroup -PropertyToQuery "PlatformToolset") {
            $propertyGroup.PlatformToolset = $targetToolset
        }

        if(Has-Property -Node $propertyGroup -PropertyToQuery "WindowsTargetPlatformVersion") {
            $propertyGroup.WindowsTargetPlatformVersion = $targetSDK
        }
    }

    $doc.Save($location)
}
