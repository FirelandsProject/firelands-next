# Guía de Testing

Esta guía explica cómo escribir, ejecutar y mantener tests en Firelands.

## Visión General

Firelands usa **GoogleTest** (gtest/gmock) para tests unitarios. El proyecto sigue un estricto flujo de **TDD (Test-Driven Development)**.

## Framework de Testing

- **GoogleTest** - Framework de tests unitarios
- **GoogleMock** - Framework de mocking para interfaces

Ambos se obtienen vía CMake FetchContent (ver `CMakeLists.txt`).

## Ubicación de Tests

```
tests/
└── unit/
    ├── shared/           # Tests para librería compartida
    ├── domain/          # Tests para capa de dominio
    ├── application/     # Tests para capa de aplicación
    ├── infrastructure/ # Tests para capa de infraestructura
    ├── tools/           # Tests para DevTools
    └── vmap/            # Tests para extractores vmap
```

## Ejecutar Tests

### Compilar con Tests Habilitados

```bash
# Configure with tests
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFIRELANDS_BUILD_TESTS=ON

# Build
ninja -C build
```

### Ejecutar Todos los Tests

```bash
ctest --test-dir build
```

### Ejecutar Suites de Tests Específicas

```bash
# Run domain tests
ctest --test-dir build -R domain

# Run application tests
ctest --test-dir build -R application

# Run infrastructure tests
ctest --test-dir build -R infrastructure

# Run shared tests
ctest --test-dir build -R shared
```

### Ejecutar un Solo Test

```bash
# Using gtest filter
ctest --test-dir build -R "CharacterServiceTests.CreateCharacter"
```

## Flujo TDD

### Red-Green-Refactor

1. **Red** - Escribir un test que falle primero
2. **Green** - Escribir código mínimo para que pase
3. **Refactor** - Limpiar mientras los tests stay green

### Ejemplo: Agregar una Nueva Característica

```cpp
// Paso 1: Red - Escribir test que falla
// tests/unit/application/CharacterServiceTests.cpp

#include <gtest/gtest.h>
#include <application/services/CharacterService.h>

TEST(CharacterService, CreateCharacter_SetsDefaultMoney) {
    // Arrange
    auto repository = std::make_unique<MockCharacterRepository>();
    auto service = CharacterService(std::move(repository));

    // Act
    auto character = service.CreateCharacter(
        1, "TestPlayer", RACE_HUMAN, CLASS_WARRIOR
    );

    // Assert - esto FALLARÁ inicialmente
    EXPECT_EQ(character.GetMoney(), 0);
}

// Paso 2: Green - Implementar código mínimo para pasar

// En CharacterService.cpp
Character CharacterService::CreateCharacter(...) {
    Character character;
    character.SetMoney(0);  // Agregar esta línea
    return character;
}

// Paso 3: Refactor - Limpiar, agregar más tests
```

## Escribiendo Tests

### Estructura de Archivo de Test

```cpp
// tests/unit/domain/MyEntityTests.cpp

#include <gtest/gtest.h>
#include <domain/models/MyEntity.h>

class MyEntityTests : public ::testing::Test {
protected:
    void SetUp() override {
        // Configuración común para todos los tests
    }

    void TearDown() override {
        // Limpieza común
    }
};

// Casos de test
TEST_F(MyEntityTests, MethodName_ExpectedBehavior) {
    // Arrange
    MyEntity entity;

    // Act
    auto result = entity.DoSomething();

    // Assert
    EXPECT_EQ(result, expected_value);
}
```

### Aserciones Comunes

| Asercón | Descripción |
|---------|-------------|
| `EXPECT_EQ(a, b)` | a == b |
| `EXPECT_NE(a, b)` | a != b |
| `EXPECT_TRUE(a)` | a es true |
| `EXPECT_FALSE(a)` | a es false |
| `EXPECT_FLOAT_EQ(a, b)` | a aproximadamente igual b |
| `EXPECT_DOUBLE_EQ(a, b)` | precisión doble |
| `EXPECT_STREQ(a, b)` | igualdad de strings |
| `EXPECT_THROW(code, type)` | code lanza excepción |
| `EXPECT_DEATH(code, regex)` | code falla (death test) |

### Test Fixtures

Usar fixtures para setup/teardown común:

```cpp
class CharacterServiceTests : public ::testing::Test {
protected:
    std::unique_ptr<ICharacterRepository> mockRepo_;
    std::unique_ptr<CharacterService> service_;

    void SetUp() override {
        mockRepo_ = std::make_unique<MockCharacterRepository>();
        service_ = std::make_unique<CharacterService>(mockRepo_.get());
    }
};
```

## Mocking

### Usando GoogleMock

```cpp
// Mock de interfaz de repositorio
#include <gmock/gmock.h>
#include <domain/repositories/ICharacterRepository.h>

class MockCharacterRepository : public ICharacterRepository {
public:
    MOCK_METHOD(std::optional<Character>, FindByGuid, (uint32 guid), (override));
    MOCK_METHOD(void, Save, (const Character&), (override));
    MOCK_METHOD(void, Delete, (uint32 guid), (override));
    MOCK_METHOD(std::vector<Character>, FindByAccount, (int accountId), (override));
};
```

### Estableciendo Expectativas

```cpp
TEST_F(CharacterServiceTests, GetCharacter_CallsRepository) {
    // Arrange
    EXPECT_CALL(*mockRepo_, FindByGuid(1))
        .WillOnce(Return(Character{...}));

    // Act
    auto result = service_->GetCharacter(1);

    // Assert
    EXPECT_TRUE(result.has_value());
}
```

### Retornando Valores

```cpp
// Return a value
EXPECT_CALL(*mockRepo_, FindByGuid)
    .WillOnce(Return(character));

// Return different values for multiple calls
EXPECT_CALL(*mockRepo_, FindByGuid)
    .WillOnce(Return(char1))
    .WillOnce(Return(char2));

// Use a lambda
EXPECT_CALL(*mockRepo_, Save)
    .WillOnce([](const Character& c) {
        // Do something
    });

// Throw exception
EXPECT_CALL(*mockRepo_, FindByGuid)
    .WillOnce(Throw(std::runtime_error("not found")));
```

## Mejores Prácticas

### Nombrado de Tests

Usar nombres descriptivos: `ClassName_Method_ExpectedBehavior`

```cpp
TEST(CharacterService, CreateCharacter_SetsDefaultLevel)
TEST(Player, TakeDamage_AppliesDamageAndReturnsHealth)
TEST(Spell, CalculateDamage_AccountsForSpellPower)
```

### Patrón AAA

1. **Arrange** - Configurar datos y objetos del test
2. **Act** - Ejecutar el método siendo probado
3. **Assert** - Verificar el resultado esperado

### Una Aserción Por Test

Enfocarse en un comportamiento por test para claridad:

```cpp
// Good - un comportamiento por test
TEST(Character, SetName_ChangesName) { }
TEST(Character, SetName_ValidatesLength) { }

// Bad - múltiples comportamientos
TEST(Character, SetName_ChangesNameAndValidatesLengthAndNotifies) { }
```

### Mantener Tests Independientes

- Cada test debe ejecutarse independientemente
- No depender del orden de ejecución
- Limpiar después de ti mismo

### Probar Casos Borde

- Input vacío
- Valores nulos
- Valores de frontera
- Valores máximos
- Input inválido

## Cobertura de Código

### Compilar con Cobertura

```bash
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DFIRELANDS_BUILD_TESTS=ON \
    -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage"

ninja -C build

# Run tests
ctest --test-dir build

# Generate coverage report (requires lcov)
lcov --capture --directory build --output-file coverage.info
lcov --remove coverage.info '*tests*' --output-file filtered.info
genhtml filtered.info --output-directory coverage_report
```

## Tests de Integración

### Contra Base de Datos Real

```cpp
class MySqlRepositoryTests : public ::testing::Test {
protected:
    std::unique_ptr<MySqlConnection> conn_;

    void SetUp() override {
        conn_ = std::make_unique<MySqlConnection>("...");
        // Run migrations if needed
    }
};

TEST_F(MySqlRepositoryTests, Save_InsertsRecord) {
    // Use real database connection
    MySqlCharacterRepository repo(conn_.get());

    Character c;
    c.SetName("Test");
    repo.Save(c);

    auto loaded = repo.FindByName("Test");
    EXPECT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->GetName(), "Test");
}
```

### Integración con Docker

Usar Docker para tests de integración:
```bash
# Start database
docker-compose up -d db

# Run integration tests
ctest --test-dir build -R integration

# Clean up
docker-compose down
```

## CI/CD

Los tests se ejecutan automáticamente en cada PR. Agregar tests a tu PR para asegurar calidad de código.

## Patrones Comunes de Test

### Probando Métodos Protegidos/Privados

Usar FRIEND_TEST o exponer vía protected:

```cpp
class MyClass {
    FRIEND_TEST(MyClassTests, InternalMethod);
protected:
    void InternalMethod();  // Testable via friend
};
```

### Probando Excepciones

```cpp
TEST(Player, EquipItem_ThrowsOnInvalidSlot) {
    Player player;
    EXPECT_THROW(player.EquipItem(999, item), std::invalid_argument);
}
```

### Probando Fecha/Hora

Cuidado con tests dependientes del tiempo. Usar inyección de dependencias o mock de tiempo.

## Solución de Problemas

### Tests No Compilan

- Revisar paths de include
- Asegurar que PCH incluye gtest/gmock

### Tests Corren Lentamente

- Revisar I/O en tests
- Usar mocks para dependencias externas

### Tests Inestables (Flaky)

- Remover dependencias de tiempo
- Usar mocks determinísticos

## Ver También

- [Configuración de Desarrollador](DEVELOPER_SETUP.md)
- [Guía de Contribución](CONTRIBUTING.md)
- [Capa de Aplicación](modules/application.md)
- [Documentación de GoogleTest](https://google.github.io/googletest/)