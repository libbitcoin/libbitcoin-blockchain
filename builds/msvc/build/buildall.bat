@ECHO OFF
ECHO.
ECHO Downloading libitcoin-blockchain dependencies from NuGet
CALL nuget.exe install ..\vs2013\libbitcoin-blockchain\packages.config
ECHO.
CALL buildbase.bat ..\vs2013\libbitcoin-blockchain.sln 12
ECHO.
PAUSE