@ECHO OFF
ECHO Downloading libbitcoin vs2017 dependencies from NuGet
CALL nuget.exe install ..\vs2017\libbitcoin-blockchain\packages.config
CALL nuget.exe install ..\vs2017\libbitcoin-blockchain-tools\packages.config
CALL nuget.exe install ..\vs2017\libbitcoin-blockchain-test\packages.config
ECHO.
ECHO Downloading libbitcoin vs2015 dependencies from NuGet
CALL nuget.exe install ..\vs2015\libbitcoin-blockchain\packages.config
CALL nuget.exe install ..\vs2015\libbitcoin-blockchain-tools\packages.config
CALL nuget.exe install ..\vs2015\libbitcoin-blockchain-test\packages.config
ECHO.
ECHO Downloading libbitcoin vs2013 dependencies from NuGet
CALL nuget.exe install ..\vs2013\libbitcoin-blockchain\packages.config
CALL nuget.exe install ..\vs2013\libbitcoin-blockchain-tools\packages.config
CALL nuget.exe install ..\vs2013\libbitcoin-blockchain-test\packages.config
