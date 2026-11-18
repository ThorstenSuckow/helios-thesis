#if !HEADLESS

        // POST
        // Clear, bufferswapping
        .beginSchedule(GameWorld::sessionState(EngineState::Running))
            .add(
                Sequential(
                    GLFWWindowCloseSystem<WindowHandle>(),
                    WindowBasedShutdownSystem<WindowHandle>(),
                    ClearAllDirtySetsSystem(),
                    ImGuiOverlayRenderSystem(imguiOverlay),
                    SwapBuffersSystem<WindowHandle>())
            )
        .endWith<DefaultGLFWPlatformManager>()

        .beginSchedule(GameWorld::sessionState(EngineState::Shutdown))
            .add<DestroySessionSystem>()
        .endSchedule();
#endif
