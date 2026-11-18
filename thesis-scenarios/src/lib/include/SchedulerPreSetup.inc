#if !HEADLESS

        scheduler.beginSchedule(GameWorld::sessionState(EngineState::Any))
            .add<EngineFlowSystem>()
        .endWith<EngineStateManager>()

        .beginSchedule(GameWorld::sessionState(EngineState::Booting))
            .add<PlatformInitSystem>()
        .endWith<DefaultGLFWPlatformManager>()

        .beginSchedule(GameWorld::sessionState(EngineState::Booted | EngineState::Running))
            .add<
                Sequential<
                    PollEventsSystem,
                    WindowCreateSystem<WindowHandle>
                >
            >()
        .endWith<DefaultGLFWPlatformManager>()

        .beginSchedule(GameWorld::sessionState(EngineState::Warmup))
            .add<
                Sequential<
                    MeshUploadSystem<MeshHandle>,
                    ShaderCompileSystem<ShaderHandle>,
                    DefaultWarmupDoneSystem
                >
            >()
        .endWith<
            Sequential<
                DefaultMeshUploadManager,
                DefaultShaderCompileManager,
                EngineStateManager
            >
        >()
        .beginSchedule(GameWorld::sessionState(EngineState::Running))
        .add<Sequential<
            YawPitchRollUpdateSystem<CameraHandle>,
            WorldTransformSystem<CameraHandle>,
            PerspectiveCameraUpdateSystem<CameraHandle>
            >
        >();

        auto& systemSchedule = scheduler.beginSchedule(GameWorld::sessionState(EngineState::Running));

#else

        auto& systemSchedule = scheduler.beginSchedule();

#endif