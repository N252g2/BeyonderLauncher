@echo off
rem Beyonder local test server. Double-click to set up (first run) and start.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0beyonder-server.ps1" %*
if errorlevel 1 pause
