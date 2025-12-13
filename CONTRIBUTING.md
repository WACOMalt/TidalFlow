# Contributing to Wave Race 64 Recompilation

Thank you for your interest in contributing! This project welcomes contributions from developers of all skill levels.

---

## How to Contribute

### 1. Report Issues

Found a bug or have a feature request? Open an issue on GitHub with:
- Clear description of the problem or feature
- Steps to reproduce (for bugs)
- Console output if relevant
- Your system information

### 2. Submit Code

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes
4. Test thoroughly
5. Commit with clear messages
6. Push and create a Pull Request

### 3. Improve Documentation

Documentation is just as valuable as code! You can:
- Fix typos or unclear explanations
- Add examples
- Improve guides
- Translate documentation

---

## Current Needs

### High Priority

- [ ] **Real Controller Input**
  - SDL controller mapping
  - Replace simulated input (0xB000)
  - Test with various controllers

- [ ] **State 2→3 Transition**
  - Investigate what triggers transition
  - May need button press or timer

- [ ] **Audio Implementation**
  - `func_800C21F4` is currently stubbed
  - Need to implement audio initialization

### Medium Priority

- [ ] **Remove Stubs Where Possible**
  - Analyze each stub to see if it can be implemented
  - Document why stubs are necessary

- [ ] **ovl_i1/i2/i3 Support**
  - Additional overlay implementations
  - More game states

### Lower Priority

- [ ] **Debug Output Cleanup**
  - Make debug output configurable
  - Add log levels

- [ ] **Performance Optimization**
  - Profile bottlenecks
  - Optimize hot paths

---

## Development Guidelines

### Code Style

- Follow existing code patterns
- Use descriptive variable names
- Comment non-obvious code
- Keep functions focused and small

### Commit Messages

```
Session N: Brief description

- Detailed change 1
- Detailed change 2

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
```

### Session Documentation

When making significant changes, create a session document:

1. Create `docs/sessions/SESSION_N_DESCRIPTION.md`
2. Document:
   - What was investigated
   - What was changed
   - Why it was changed
   - Test results
   - Next steps

### Testing

Before submitting:
1. Build successfully: `cmake --build build`
2. Run the game: `./build/WaveRace64Recompiled`
3. Verify no regressions
4. Test with timeout: `timeout 30 ./build/WaveRace64Recompiled`

---

## AI-Assisted Development

This project uses AI assistance (Claude Code). When contributing with AI:

1. **Provide Context**
   - Share relevant session logs
   - Include error messages
   - Explain what you've tried

2. **Document AI Interactions**
   - If AI helped solve a problem, note it
   - Include the reasoning process
   - Share insights for future reference

3. **Verify AI Suggestions**
   - Always test AI-generated code
   - Understand why a fix works
   - Don't blindly accept solutions

---

## Project Structure

```
wave-race-64-recomp/
├── waverace-recomp/
│   ├── waverace.toml        # N64Recomp config
│   ├── waverace.syms.toml   # Symbol definitions
│   ├── src/game/
│   │   └── waverace_stubs.cpp  # Custom stubs
│   └── lib/
│       ├── N64ModernRuntime/ # Runtime library
│       └── rt64/             # Graphics renderer
│
├── docs/
│   ├── sessions/            # Session logs
│   └── *.md                 # Guides
│
└── N64Recomp/               # Recompilation tool
```

### Key Files to Know

| File | Purpose |
|------|---------|
| `waverace.toml` | Main N64Recomp configuration |
| `waverace.syms.toml` | Function/overlay definitions |
| `waverace_stubs.cpp` | Custom implementations |
| `docs/LESSONS_LEARNED.md` | Key debugging insights |

---

## Getting Help

- **Issues**: Open a GitHub issue
- **Discussions**: Use GitHub Discussions for questions
- **Session Logs**: Read `docs/sessions/` for context

---

## License

By contributing, you agree that your contributions will be licensed under the same terms as the project.

---

*Thank you for helping make Wave Race 64 run on modern systems!*
