from pathlib import Path
p = Path(r"C:\turnsei\trnsei\trnsei\trnsei.vcxproj")
t = p.read_text(encoding="utf-8")
old = """  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <IntrinsicFunctions>true</IntrinsicFunctions>
      <SDLCheck>true</SDLCheck>
      <PreprocessorDefinitions>NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <ConformanceMode>true</ConformanceMode>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
      <GenerateDebugInformation>true</GenerateDebugInformation>
    </Link>
  </ItemDefinitionGroup>"""
new = """  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <IntrinsicFunctions>true</IntrinsicFunctions>
      <SDLCheck>true</SDLCheck>
      <PreprocessorDefinitions>NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <ConformanceMode>true</ConformanceMode>
      <AdditionalIncludeDirectories>$(SolutionDir)\\Extend\\GLFW\\glfw-3.4.bin.WIN64\\include\\GLFW;$(SolutionDir)\\Extend\\GLFW\\glfw-3.4.bin.WIN64\\include;$(SolutionDir)\\Extend\\GLEW\\glew-2.3.1\\include\\GL;$(SolutionDir)\\Extend\\GLM\\glm;$(ProjectDir)imgui;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <AdditionalLibraryDirectories>$(SolutionDir)\\Extend\\GLEW\\glew-2.3.1\\lib\\Release\\x64;$(SolutionDir)\\Extend\\GLFW\\glfw-3.4.bin.WIN64\\lib-vc2022;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
      <AdditionalDependencies>opengl32.lib;glew32s.lib;glfw3.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>"""
if old not in t:
    raise SystemExit("old block not found")
t = t.replace(old, new, 1)
img = """    <ClCompile Include=\"imgui\\imgui.cpp\" />
    <ClCompile Include=\"imgui\\imgui_draw.cpp\" />
    <ClCompile Include=\"imgui\\imgui_tables.cpp\" />
    <ClCompile Include=\"imgui\\imgui_widgets.cpp\" />
    <ClCompile Include=\"imgui\\imgui_impl_glfw.cpp\" />
    <ClCompile Include=\"imgui\\imgui_impl_opengl3.cpp\" />"""
needle = '    <ClCompile Include="src\\game\\CombatSystem.cpp" />'
if needle not in t:
    raise SystemExit("needle not found")
t = t.replace(needle, needle + "\n" + img)
p.write_text(t, encoding="utf-8")
print("patched")
