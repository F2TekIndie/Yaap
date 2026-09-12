# Publishing Yaap on GitHub

This repository is prepared for GitHub without committing build products, user
data, credentials, local dependency trees, or packaged distributions.

## Before the first push

1. Confirm that the `LGPL-3.0-only` project license in `LICENSE` matches the
   intended publication terms. Dependency licenses remain separate.
2. Review the complete change set with `git status` and `git diff`.
3. Run `git diff --check`, the appropriate build/test preset, and the
   `yaap_release_gate` target.
4. Create an empty GitHub repository. Do not ask GitHub to generate another
   README, `.gitignore`, or license file over this history.

Check the branch and remotes before publishing; an integration branch is not
necessarily the intended default branch:

```sh
git branch --show-current
git remote -v
git tag --list
```

For a new repository whose chosen default branch is `master`:

```powershell
git remote add origin https://github.com/<owner>/Yaap.git
git push -u origin master
```

To use `main` instead, rename it before the first push:

```powershell
git branch -m main
git remote add origin https://github.com/<owner>/Yaap.git
git push -u origin main
```

Replace `<owner>` with the GitHub user or organization. Do not run both branch
variants.

## Recommended repository settings

- Enable GitHub Actions and private vulnerability reporting.
- Protect the default branch and require pull requests and passing build checks.
- Enable Dependabot security updates and review dependency updates before merging.
- Add a concise description and topics such as `music-player`, `cpp20`, `qt6`,
  `ffmpeg`, `miniaudio`, and `cmake`.
- Add releases only from tested, versioned commits and attach CI-produced packages.

CI builds and tests Windows, Linux, and macOS, also runs a Linux sanitizer job,
validates release metadata, stages an installation, and uploads generated packages.
