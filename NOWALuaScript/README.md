# NOWALuaScript

NOWALuaScript is a Qt-based C++ application designed for managing, editing, and running Lua scripts. It provides seamless integration with other applications, such as game engines, via an XML-based file communication system.
This document outlines the key features, installation, usage, and communication protocol of the application.

## Table of Contents
- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [File Communication Protocol](#file-communication-protocol)
  - [Case 1: Opening Lua Scripts](#case-1-opening-lua-scripts)
  - [Case 2: Receiving Runtime Errors](#case-2-receiving-runtime-errors)
  - [Case 3: Back Communication](#case-3-back-communication)
- [Lua API Integration](#lua-api-integration)
- [Instance Management](#instance-management)
- [Best Practices](#best-practices)

## Features
- **File Watching**: Monitors for the creation or modification of XML files to trigger actions within NOWALuaScript.
- **Integrated Lua Editor**: Open Lua script files in tabs and make them active based on received paths.
- **Runtime Error Detection**: Receive and display runtime errors for better debugging and error handling.
- **Communication with External Applications**: Communicate with game engines or other software via an XML protocol.
- **API Parsing**: Load custom Lua APIs for auto-completion and type inference within the editor.
- **Single Instance Control**: Ensures that only one instance of the application runs at a time using a lock file mechanism.

## Installation
To install and run NOWALuaScript:
1. Clone the repository and build the project using Qt Creator or the command line.
2. Ensure all necessary Qt dependencies are installed.

## Usage
1. **Launching the Application**: Run the executable from the command line or directly through your development environment.
2. **File Monitoring**: The application monitors the creation and modification of the `lua_script_data.xml` file and performs actions based on the parsed XML content.
3. **Managing Tabs**: The application will open new Lua scripts in a tab when provided with the file path in `lua_script_data.xml`.

## File Communication Protocol
NOWALuaScript communicates with external applications (e.g., game engines) through a specific XML structure. Below are the supported cases:

### Case 1: Opening Lua Scripts
To open a Lua script in NOWALuaScript, the external application should create an XML file with the following structure:
```xml
<Message>
    <MessageId>LuaScriptPath</MessageId>
    <FilePath>C:/Users/lukas/Documents/NOWA-Engine/media/Projects/Project1/Scene1_barrel_0.lua</FilePath>
</Message>
```
Behavior: If the provided file path is already opened in a tab, the application will switch to that tab. Otherwise, it will open a new tab with the specified script.

### Case 2: Receiving Runtime Errors
To report runtime errors that cannot be detected during script parsing:

```xml
<Message>
    <MessageId>LuaRuntimeErrors</MessageId>
    <error line="14" start="-1" end="-1">object is nil</error>
</Message>
```
Clearing Errors: Send an empty error tag to clear all runtime errors:
```xml
<Message>
    <MessageId>LuaRuntimeErrors</MessageId>
    <error line="-1" start="-1" end="-1"></error>
</Message>
```

### Case 3: Back Communication
NOWALuaScript can send commands back to the external application for tasks such as starting or stopping simulations:

```xml
<Message>
    <MessageId>Play</MessageId>
</Message>

<Message>
    <MessageId>Stop</MessageId>
</Message>
```

## Lua API Integration
NOWALuaScript supports loading custom Lua API files to enhance the auto-completion and type-checking capabilities. The Lua API file should:

Be formatted as a Lua table ending with .lua.
Include class definitions, methods, and type information.
Example API File:

```lua
return {

	AiComponent =
	{
		type = "class",
		description = "Base class for ai components with some attributes. Note: The game Object must have a PhysicsActiveComponent.",
		inherits = "GameObjectComponent",
		childs = 
		{
			setActivated =
			{
				type = "method",
				description = "Sets the ai component is activated. If true, it will move according to its specified behavior.",
				args = "(boolean activated)",
				returns = "(nil)",
				valuetype = "nil"
			},
			isActivated =
			{
				type = "function",
				description = "Gets whether ai component is activated. If true, it will move according to its specified behavior.",
				args = "()",
				returns = "(boolean)",
				valuetype = "boolean"
			},
			setRotationSpeed =
			{
				type = "method",
				description = "Sets the rotation speed for the ai controller game object.",
				args = "(number rotationSpeed)",
				returns = "(nil)",
				valuetype = "nil"
			},
			getRotationSpeed =
			{
				type = "function",
				description = "Gets the rotation speed for the ai controller game object.",
				args = "()",
				returns = "(number)",
				valuetype = "number"
			}
		}
	},
	AnimationBlender =
	{
		type = "class",
		description = "This class can be used for more complex animations and transitions between them.",
		childs = 
		{
			init1 =
			{
				type = "method",
				description = "Inits the animation blender, also sets and start the first current animation id.",
				args = "(AnimID animationId, boolean loop)",
				returns = "(nil)",
				valuetype = "nil"
			},
			init2 =
			{
				type = "method",
				description = "Inits the animation blender, also sets and start the first current animation name.",
				args = "(string animationName, boolean loop)",
				returns = "(nil)",
				valuetype = "nil"
			},
			getAllAvailableAnimationNames =
			{
				type = "function",
				description = "Gets all available animation names. A string list with all animation names. If none found, empty list will be delivered.",
				args = "(boolean skipLogging)",
				returns = "(Table[string])",
				valuetype = "Table[string]"
			}
		}
	},
	GameObjectController =
	{
		type = "class",
		description = "The game object controller manages all game objects.",
		childs = 
		{
			deleteDelayedGameObject =
			{
				type = "method",
				description = "Deletes a game object by the given id after a delay of milliseconds.",
				args = "(string gameObjectId, number delayMS)",
				returns = "(nil)",
				valuetype = "nil"
			},
			castGameObject =
			{
				type = "function",
				description = "Casts an incoming type from function for lua auto completion.",
				args = "(GameObject other)",
				returns = "(GameObject)",
				valuetype = "GameObject"
			},
			castAnimationComponent =
			{
				type = "function",
				description = "Casts an incoming type from function for lua auto completion.",
				args = "(AnimationComponent other)",
				returns = "(AnimationComponent)",
				valuetype = "AnimationComponent"
			}
		}
	},
	Vector3 =
	{
		type = "class",
		description = "Vector3 class.",
		childs = 
		{
			x =
			{
				type = "value"
			},
			y =
			{
				type = "value"
			},
			z =
			{
				type = "value"
			},
			crossProduct =
			{
				type = "function",
				description = "Gets the cross product between two vectors.",
				args = "(Vector3 other)",
				returns = "(Vector3)",
				valuetype = "Vector3"
			}
		}
	}
}
```

Also take a look at the NOWA_Api.lua file as orientation. 

## Instance Management
To ensure only one instance of the application runs at a time, NOWALuaScript creates a NOWALuaScript.running file in the executable's bin directory when launched. This file will be deleted upon closing the application.

## Best Practices
Use castXXX Functions: Define functions with castXXX (e.g., castContact) so the editor can infer types.
Use getXXX Naming: Use getXXX patterns for functions to ensure type inference works correctly in function chains.
Example Usage in Lua:

```lua
Scene1_barrel_0["onContactOnce"] = function(gameObject0, gameObject1, contact)
    contact = AppStateManager:getGameObjectController():castContact(contact)
end
```
By following these practices, you can maximize the integration of NOWALuaScript with external applications and take full advantage of its powerful scripting and auto-completion features.