# Guía de Contribución

Esta guía describe cómo contribuir a Firelands, incluyendo el flujo de trabajo de desarrollo, estándares de código y mejores prácticas.

## Prerrequisitos

Antes de contribuir, asegúrate de entender:

1. **Arquitectura Hexagonal** - Ver [Resumen de Arquitectura](modules/README.md)
2. **Flujo TDD** - Escribir tests primero, luego la implementación
3. **Sistema de Build** - CMake + Ninja (ver [Configuración de Desarrollador](DEVELOPER_SETUP.md))
4. **Idioma** - Todo el código, comentarios y git en inglés

## Flujo de Trabajo de Desarrollo

### 1. Encontrar Trabajo

- Revisar el [Roadmap](ROADMAP.md) para características planeadas
- Buscar issues de GitHub etiquetados como `good first issue`
- Revisar vacíos en la documentación existente

### 2. Comenzar el Trabajo

```bash
# Crear una nueva rama para tu característica
git checkout -b feature/mi-nueva-caracteristica
# O para arreglar bugs
git checkout -b fix/descripcion-del-bug
```

Convención de nombres de ramas:
- `feature/` - Nuevas características
- `fix/` - Arreglos de bugs
- `refactor/` - Refactorización de código
- `docs/` - Solo documentación

### 3. Proceso de Desarrollo

1. **Escribir tests primero** (TDD) - Ver [Guía de Testing](TESTING.md)
2. **Implementar la característica**
3. **Verificar que los tests pasen**
4. **Ejecutar linter/formateador** (si está configurado)
5. **Hacer commit con mensajes claros**

### 4. Mensajes de Commit

Seguir el patrón:
```
<tipo>(<alcance>): <descripción>

[cuerpo opcional]
```

Tipos: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`

Ejemplos:
```
feat(auth): add SRP6a password reset flow
fix(world): correct spell damage calculation for area spells
docs(database): document new migration schema
refactor(spell): extract damage formula to domain layer
```

### 5. Enviar Cambios

1. Subir tu rama:
   ```bash
   git push origin feature/mi-nueva-caracteristica
   ```

2. Crear un Pull Request en GitHub
3. Llenar la plantilla de PR
4. Esperar revisión de código

## Estándares de Código

### Idioma

- **Código**: Solo inglés (variables, funciones, clases)
- **Comentarios**: Solo inglés
- **Mensajes de git**: Solo inglés
- **Documentación**: Inglés para EN/, Español para ES/

### Estilo C++

- Estándar **C++20** requerido
- Usar `snake_case` para variables y funciones
- Usar `PascalCase` para tipos y clases
- Usar `UPPER_SNAKE_CASE` para constantes y enums
- Usar `kebab-case` para nombres de archivos

Ejemplo:
```cpp
class MyClass {
public:
    void DoSomething();
    int CalculateValue() const;

private:
    int internal_state_ = 0;
    static constexpr int kMaxValue = 100;
};

void MyClass::DoSomething() {
    for (auto& item : items_) {
        item.Process();
    }
}
```

### Headers

- Usar include guards o `#pragma once`
- Ordenar includes: 1) header asociado, 2) proyecto, 3) sistema
- Usar forward declarations cuando sea posible

```cpp
// associated.h
#ifndef PROJECT_MODULE_COMPONENT_H
#define PROJECT_MODULE_COMPONENT_H

#include <module/other_component.h>  // project
#include <vector>                     // system
#include <memory>                     // system

// Forward declarations
class IRepository;

class Component {
    // ...
};
#endif
```

### Reglas de Arquitectura

**Dirección de dependencias** (hexagonal):
```
Infraestructura → Aplicación → Dominio → Compartido
```

- `domain/` NO DEBE importar de `application/` o `infrastructure/`
- Usar **puertos** (interfaces) para inversión de dependencias
- Implementar **adaptadores** en `infrastructure/`

Ejemplo:
```cpp
// domain/repositories/ICharacterRepository.h
#pragma once

#include <domain/models/Character.h>
#include <optional>

class ICharacterRepository {
public:
    virtual ~ICharacterRepository() = default;
    virtual std::optional<Character> FindByGuid(uint32 guid) = 0;
    virtual void Save(const Character& character) = 0;
};

// infrastructure/persistence/MySqlCharacterRepository.h
#pragma once

#include <domain/repositories/ICharacterRepository.h>
#include <mysql_connection.h>  // solo infraestructura

class MySqlCharacterRepository : public ICharacterRepository {
    // implementación...
};
```

### Manejo de Errores

- Usar excepciones para situaciones excepcionales
- Devolver `std::optional<T>` para operaciones que pueden fallar
- Usar tipos resultado para operaciones con múltiples resultados

### Logging

- Usar `Logger` de `shared/Logger.h`
- Incluir contexto en los mensajes de log
- Usar niveles apropiados: `trace`, `debug`, `info`, `warn`, `error`

```cpp
#include <shared/Logger.h>

LOG_INFO("Player {} logged in from {}", player.GetName(), ip_address);
LOG_ERROR("Database connection failed: {}", error.what());
```

## Estándares SQL

### Archivos de Migración

- Ubicar en `sql/migrations/`
- Prefijar con número de orden: `001_`, `002_`, etc.
- Usar operaciones idempotentes
- Incluir comentarios explicando el cambio

```sql
-- Add new column for future feature
-- See issue #123
ALTER TABLE account
    ADD COLUMN IF NOT EXISTS `some_feature_flag` TINYINT DEFAULT 0;

-- Add index for better query performance
CREATE INDEX IF NOT EXISTS idx_character_account
    ON characters(account);
```

### Código de Base de Datos

- Todo SQL en la capa de infraestructura
- Usar consultas parametrizadas (nunca concatenación de strings)
- Seguir convenciones de nombres de esquemas existentes

## Estándares LUA

- Los scripts van en `scripts/lua/`
- Usar nombres de funciones descriptivos
- Seguir patrones de scripts existentes (ver [Scripting LUA](LUA_SCRIPTING.md))

## Estándares de Testing

### Tests Unitarios

- Usar framework GoogleTest
- Testear una cosa por caso de test
- Usar nombres de tests descriptivos
- Seguir patrón AAA (Arrange, Act, Assert)

```cpp
TEST(CharacterService, CreateCharacter_SetsDefaultValues) {
    // Arrange
    auto service = CreateTestService();

    // Act
    auto character = service.CreateCharacter(account_id, "TestChar", RACE_HUMAN, CLASS_WARRIOR);

    // Assert
    EXPECT_EQ(character.GetName(), "TestChar");
    EXPECT_EQ(character.GetLevel(), 1);
    EXPECT_EQ(character.GetMoney(), 0);
}
```

### Tests de Integración

- Usar base de datos real cuando sea posible
- Mockear servicios externos (servidor world, REST)
- Limpiar datos de test después de los tests

## Estándares de Documentación

### Agregar Documentación

1. Crear o actualizar archivo en `docs/EN/` (Inglés) y `docs/ES/` (Español)
2. Usar encabezados claros y descriptivos
3. Incluir ejemplos de código donde sea útil
4. Mantener documentación sincronizada con el código

### Archivos de Documentación

- **Docs de módulos**: `docs/EN/modules/*.md`
- **Guías**: `docs/EN/*.md` (Developer Setup, Database Schema, etc.)
- **Docs de API**: En línea en los headers

## Tipos de Contribución Comunes

### Agregar una Nueva Característica

1. Crear rama de característica
2. Agregar modelo de dominio (si es necesario)
3. Agregar interfaz de repositorio
4. Agregar capa de servicio
5. Agregar implementación de infraestructura
6. Agregar tests
7. Documentar la característica
8. Submit PR

### Arreglar un Bug

1. Crear rama de fix
2. Escribir test que falla primero
3. Arreglar el bug
4. Verificar que el test pase
5. Submit PR

### Agregar una Migración de Base de Datos

1. Crear archivo de migración en `sql/migrations/`
2. Escribir SQL idempotente
3. Probar en base de datos local
4. Actualizar SQL bundled (`merge-migrations` target)
5. Documentar el cambio en la guía de esquema

### Agregar un Comando GM

1. Agregar permiso a `Permissions.h`
2. Registrar comando en `CommandService.cpp`
3. Implementar método handler
4. Agregar tests para el comando
5. Documentar en [Administración GM](modules/gm-administration.md)

## Guías de Revisión de Código

### Para Revisores

- Ser constructivo y respetuoso
- Enfocarse en lógica, arquitectura y estilo
- Sugerir mejoras, no exigir
- Aprobar cuando esté listo

### Para Autores

- Responder a todos los comentarios
- No tomar la crítica de manera personal
- Explicar tus decisiones
- Mantener PRs enfocados y de tamaño razonable

## Obtener Ayuda

- **Documentación**: Revisar carpeta `docs/`
- **Issues**: Abrir en GitHub
- **Discusión**: Usar GitHub Discussions
- **Referencia**: Ver `firelands-cata-ref/` para ejemplos de implementación

## Reconocimiento

Los contribuyentes son reconocidos en el proyecto. ¡Gracias por tu trabajo!