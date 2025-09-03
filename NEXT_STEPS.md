## Next Steps Roadmap

1. Enable ImGui docking (switch to docking-enabled build, set io.ConfigFlags |= Docking). 
2. Serialize entire effect chain (new preset format with array of effects + params). 
3. Implement additional effects (Clear/Blur/ColorMod) and blend modes. 
4. Add FPS timing & frame pacing controls. 
5. Audio device selector ImGui combo + gain control. 
6. Fullscreen preview window & screenshot capture. 
7. Replace raw includes with proper moved source (remove bridging includes). 
8. Add unit tests for preset serialization logic. 
9. Plan scripting engine reintegration (EEL or Lua). 
10. Prepare packaging: minimal README, license notices, build instructions. 

Short-term priority: 1,2,3 to reach functional preset editor parity.
